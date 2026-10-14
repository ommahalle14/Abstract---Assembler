// worklet2.cpp -- x86-32 encoder. Consumes only enums/integers; no text handling.
#include "worklet2.h"

#include <fstream>

namespace w2 {
namespace {

const size_t kUndefined = static_cast<size_t>(-1);

struct Fixup {
    size_t  pos;        // offset of the 32-bit field in the output
    LabelId label;
    size_t  instr;      // for error reporting
    bool    absolute;   // true: baseAddress + target;  false: rel32 (target - end of field)
};

// ---- byte emission (little-endian, independent of host endianness) --------

inline void put8(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
}
inline void put32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
    o.push_back(static_cast<uint8_t>(v >> 16));
    o.push_back(static_cast<uint8_t>(v >> 24));
}
inline void patch32(std::vector<uint8_t>& o, size_t pos, uint32_t v) {
    o[pos + 0] = static_cast<uint8_t>(v);
    o[pos + 1] = static_cast<uint8_t>(v >> 8);
    o[pos + 2] = static_cast<uint8_t>(v >> 16);
    o[pos + 3] = static_cast<uint8_t>(v >> 24);
}

// ---- operand shape predicates ----------------------------------------------

inline uint8_t code(Reg r)                    { return static_cast<uint8_t>(r); }
inline bool isGpr(Reg r)                      { return code(r) < 8; }
inline bool isReg(const Operand& o)           { return o.kind == OperandKind::REG && isGpr(o.reg); }
inline bool isMem(const Operand& o)           { return o.kind == OperandKind::MEM || o.kind == OperandKind::MEMLABEL; }
inline bool isImm(const Operand& o)           { return o.kind == OperandKind::IMM; }
inline bool isRM(const Operand& o)            { return isReg(o) || isMem(o); }
inline bool fitsInt8(int32_t v)               { return v >= -128 && v <= 127; }

// ---- ModRM / SIB / displacement ---------------------------------------------
// Emits the addressing bytes for `rm` (a REG, MEM or MEMLABEL operand).
// `regField` goes in ModRM.reg: either the other register operand, or an
// opcode /digit.
Status emitModRM(std::vector<uint8_t>& out, uint32_t regField, const Operand& rm,
                 std::vector<Fixup>& fixups, size_t idx) {
    if (rm.kind == OperandKind::REG) {                      // mod = 11
        put8(out, 0xC0 | (regField << 3) | code(rm.reg));
        return Status::OK;
    }

    if (rm.kind == OperandKind::MEMLABEL) {                 // [abs32 of label]
        put8(out, (regField << 3) | 0x05);                  // mod=00, rm=101
        Fixup f = { out.size(), rm.label, idx, true };
        fixups.push_back(f);
        put32(out, 0);                                      // patched later
        return Status::OK;
    }

    const MemOperand& m = rm.mem;
    const bool hasBase  = m.base  != Reg::NONE;
    const bool hasIndex = m.index != Reg::NONE;
    if ((hasBase && !isGpr(m.base)) || (hasIndex && !isGpr(m.index))) return Status::BAD_OPERANDS;

    uint32_t scaleBits = 0;
    if (hasIndex) {
        if (m.index == Reg::ESP) return Status::BAD_INDEX_REG;
        switch (m.scale) {
            case 1: scaleBits = 0; break;
            case 2: scaleBits = 1; break;
            case 4: scaleBits = 2; break;
            case 8: scaleBits = 3; break;
            default: return Status::BAD_SCALE;
        }
    }

    if (!hasBase && !hasIndex) {                            // [disp32]
        put8(out, (regField << 3) | 0x05);
        put32(out, static_cast<uint32_t>(m.disp));
        return Status::OK;
    }

    if (!hasBase) {                                         // [index*scale + disp32]
        put8(out, (regField << 3) | 0x04);                  // rm=100 -> SIB follows
        put8(out, (scaleBits << 6) | (code(m.index) << 3) | 0x05);  // base=101 -> none
        put32(out, static_cast<uint32_t>(m.disp));
        return Status::OK;
    }

    // Base present. EBP as base can't use mod=00 (that slot means "no base"),
    // so it always carries an explicit displacement, even a zero one.
    uint32_t mod;
    if (m.disp == 0 && m.base != Reg::EBP) mod = 0;
    else if (fitsInt8(m.disp))             mod = 1;
    else                                   mod = 2;

    // ESP as base (or any index) requires a SIB byte.
    const bool needSib = hasIndex || m.base == Reg::ESP;

    put8(out, (mod << 6) | (regField << 3) | (needSib ? 4u : code(m.base)));
    if (needSib) {
        put8(out, (scaleBits << 6) | ((hasIndex ? code(m.index) : 4u) << 3) | code(m.base));
    }
    if (mod == 1)      put8(out, static_cast<uint32_t>(m.disp));
    else if (mod == 2) put32(out, static_cast<uint32_t>(m.disp));
    return Status::OK;
}

// ---- opcode table for the ALU group (ADD/OR/AND/SUB/XOR/CMP) -----------------
// rmR = "op r/m32, r32", rRm = "op r32, r/m32", ext = /digit for 81 and 83.
// The short "op EAX, imm32" opcode is always rmR + 4.
bool aluForm(Op op, uint8_t& rmR, uint8_t& rRm, uint8_t& ext) {
    switch (op) {
        case Op::ADD: rmR = 0x01; rRm = 0x03; ext = 0; return true;
        case Op::OR:  rmR = 0x09; rRm = 0x0B; ext = 1; return true;
        case Op::AND: rmR = 0x21; rRm = 0x23; ext = 4; return true;
        case Op::SUB: rmR = 0x29; rRm = 0x2B; ext = 5; return true;
        case Op::XOR: rmR = 0x31; rRm = 0x33; ext = 6; return true;
        case Op::CMP: rmR = 0x39; rRm = 0x3B; ext = 7; return true;
        default:      return false;
    }
}

//
    uint8_t rmR, rRm, ext;
    if (!aluForm(in.op, rmR, rRm, ext)) return Status::BAD_OPCODE;
    if (n != 2) return Status::BAD_OPERANDS;

    if (isRM(a) && isImm(b)) {
        const bool short8 = fitsInt8(b.imm);
        // op EAX, imm32  (opcode+4, no ModRM) -- the 5-byte short form that
        // Worklet 1's Pass 1 assumes whenever the immediate doesn't fit in 8 bits.
        if (!short8 && isReg(a) && a.reg == Reg::EAX) {
            put8(out, rmR + 4u);
            put32(out, static_cast<uint32_t>(b.imm));
            return Status::OK;
        }
        put8(out, short8 ? 0x83u : 0x81u);                  // 83 /d ib  or  81 /d id
        Status s = emitModRM(out, ext, a, fixups, idx);
        if (s != Status::OK) return s;
        if (short8) put8(out, static_cast<uint32_t>(b.imm));
        else        put32(out, static_cast<uint32_t>(b.imm));
        return Status::OK;
    }
    if (isReg(a) && isRM(b)) {                              // op r32, r/m32   /r
        put8(out, rRm);
        return emitModRM(out, code(a.reg), b, fixups, idx);
    }
    if (isMem(a) && isReg(b)) {                             // op r/m32, r32   /r
        put8(out, rmR);
        return emitModRM(out, code(b.reg), a, fixups, idx);
    }
    return Status::BAD_OPERANDS;
}

Result fail(Status s, size_t idx) {
    Result r = { s, idx };
    return r;
}

//

} // namespace

const char* statusText(Status s) {
    switch (s) {
        case Status::OK:                 return "OK";
        case Status::BAD_OPCODE:         return "opcode not handled";
        case Status::BAD_OPERANDS:       return "wrong operand count or unsupported operand combination";
        case Status::BAD_SCALE:          return "memory scale must be 1, 2, 4 or 8";
        case Status::BAD_INDEX_REG:      return "ESP cannot be an index register";
        case Status::LABEL_OUT_OF_RANGE: return "label ID out of range";
        case Status::DUPLICATE_LABEL:    return "label defined twice";
        case Status::UNDEFINED_LABEL:    return "label referenced but never defined";
    }
    return "unknown status";
}

Result encode(const Program& prog, std::vector<uint8_t>& out, std::vector<size_t>* instrOffsets) {
    out.clear();
    if (instrOffsets) instrOffsets->clear();

    Result r = encodeImpl(prog, out, instrOffsets);
    if (r.status != Status::OK) {
        out.clear();
        if (instrOffsets) instrOffsets->clear();
    }
    return r;
}


