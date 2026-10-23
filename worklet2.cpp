// worklet2.cpp -- x86-32 encoder. Consumes only enums/integers; no text handling.
#include "worklet2.h"

#include <fstream>
#include <iostream>

namespace w2 {
namespace {

const size_t kUndefined = static_cast<size_t>(-1);

enum FixKind { FX_REL32, FX_ABS32, FX_REL8 };

struct Fixup {
    size_t  pos;        // offset of the field in the output
    LabelId label;
    size_t  instr;      // for error reporting
    FixKind kind;       // REL32: target - end of field | ABS32: baseAddress + target + addend | REL8: like REL32, 1 byte
    int32_t addend;     // ABS32 only (the displacement in [label + disp])
};

// ---- byte emission (little-endian, independent of host endianness) --------

inline void put8(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
}
inline void put16(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
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

// ---- operand sizes (bits) ---------------------------------------------------
const unsigned kConflict = 99;
inline unsigned regBits(const Operand& o)     { return o.size ? o.size : 32u; }
inline bool validSize(unsigned s)             { return s == 8 || s == 16 || s == 32; }
inline bool isAcc(const Operand& o)           { return isReg(o) && o.reg == Reg::EAX; }   // AL / AX / EAX
inline bool isCL(const Operand& o)            { return isReg(o) && o.reg == Reg::ECX && regBits(o) == 8; }

// Width shared by every register / sized-memory operand in ops[0..n).
// 0 = none of them states one, kConflict = they disagree.  IMM/LABEL are ignored.
unsigned sizeOf(const Operand* ops, unsigned n) {
    unsigned s = 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned v = 0;
        if (ops[i].kind == OperandKind::REG)                          v = regBits(ops[i]);
        else if (isMem(ops[i]) && ops[i].size != 0)                   v = ops[i].size;
        if (v == 0) continue;
        if (s != 0 && s != v) return kConflict;
        s = v;
    }
    return s;
}
// Same, but the result must be a usable 8/16/32; otherwise BAD_SIZE.
inline bool needSize(const Operand* ops, unsigned n, unsigned& S) {
    S = sizeOf(ops, n);
    return validSize(S);
}

inline void prefix66(std::vector<uint8_t>& o, unsigned S) { if (S == 16) put8(o, 0x66); }

// Immediate handling for an operand of S bits.  The value is accepted either as
// unsigned (0..255, 0..65535) or signed, and normalised to its signed meaning.
inline bool immFits(int32_t v, unsigned S) {
    if (S == 8)  return v >= -128   && v <= 255;
    if (S == 16) return v >= -32768 && v <= 65535;
    return true;
}
inline int32_t immNorm(int32_t v, unsigned S) {
    if (S == 8)  return static_cast<int8_t>(static_cast<uint8_t>(v));
    if (S == 16) return static_cast<int16_t>(static_cast<uint16_t>(v));
    return v;
}
inline void putImm(std::vector<uint8_t>& o, int32_t v, unsigned S) {
    if (S == 8) put8(o, static_cast<uint32_t>(v));
    else if (S == 16) put16(o, static_cast<uint32_t>(v));
    else put32(o, static_cast<uint32_t>(v));
}

// 32-bit displacement field; if the operand names a label it is back-patched.
void putDisp32(std::vector<uint8_t>& out, const MemOperand& m,
               std::vector<Fixup>& fixups, size_t idx) {
    if (m.label != NO_LABEL) {
        Fixup f = { out.size(), m.label, idx, FX_ABS32, m.disp };
        fixups.push_back(f);
        put32(out, 0);
    } else {
        put32(out, static_cast<uint32_t>(m.disp));
    }
}

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
        Fixup f = { out.size(), rm.label, idx, FX_ABS32, 0 };
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

    if (!hasBase && !hasIndex) {                            // [disp32]  or  [label + disp]
        put8(out, (regField << 3) | 0x05);
        putDisp32(out, m, fixups, idx);
        return Status::OK;
    }

    if (!hasBase) {                                         // [index*scale + disp32]
        put8(out, (regField << 3) | 0x04);                  // rm=100 -> SIB follows
        put8(out, (scaleBits << 6) | (code(m.index) << 3) | 0x05);  // base=101 -> none
        putDisp32(out, m, fixups, idx);
        return Status::OK;
    }

    // Base present. EBP as base can't use mod=00 (that slot means "no base"),
    // so it always carries an explicit displacement, even a zero one.
    uint32_t mod;
    if (m.label != NO_LABEL)               mod = 2;         // label: value unknown yet -> disp32
    else if (m.disp == 0 && m.base != Reg::EBP) mod = 0;
    else if (fitsInt8(m.disp))             mod = 1;
    else                                   mod = 2;

    // ESP as base (or any index) requires a SIB byte.
    const bool needSib = hasIndex || m.base == Reg::ESP;

    put8(out, (mod << 6) | (regField << 3) | (needSib ? 4u : code(m.base)));
    if (needSib) {
        put8(out, (scaleBits << 6) | ((hasIndex ? code(m.index) : 4u) << 3) | code(m.base));
    }
    if (mod == 1)      put8(out, static_cast<uint32_t>(m.disp));
    else if (mod == 2) putDisp32(out, m, fixups, idx);
    return Status::OK;
}

// ---- opcode table for the ALU group (ADD/OR/AND/SUB/XOR/CMP) -----------------
// rmR = "op r/m32, r32", rRm = "op r32, r/m32", ext = /digit for 80, 81 and 83.
// The 8-bit forms are the same opcodes minus 1 (r/m8,r8 = rmR-1, r8,r/m8 = rRm-1);
// the short "op AL/AX/EAX, imm" opcode is rmR + 4 (8-bit: rmR + 3).
bool aluForm(Op op, uint8_t& rmR, uint8_t& rRm, uint8_t& ext) {
    switch (op) {
        case Op::ADD: rmR = 0x01; rRm = 0x03; ext = 0; return true;
        case Op::OR:  rmR = 0x09; rRm = 0x0B; ext = 1; return true;
        case Op::ADC: rmR = 0x11; rRm = 0x13; ext = 2; return true;
        case Op::SBB: rmR = 0x19; rRm = 0x1B; ext = 3; return true;
        case Op::AND: rmR = 0x21; rRm = 0x23; ext = 4; return true;
        case Op::SUB: rmR = 0x29; rRm = 0x2B; ext = 5; return true;
        case Op::XOR: rmR = 0x31; rRm = 0x33; ext = 6; return true;
        case Op::CMP: rmR = 0x39; rRm = 0x3B; ext = 7; return true;
        default:      return false;
    }
}

// ---- tables for the added instruction set -------------------------------------
// Opcode of every added instruction that is exactly one byte long.
uint8_t singleByteOpcode(Op op) {
    switch (op) {
        case Op::HLT:    return 0xF4;
        case Op::CLC:    return 0xF8;
        case Op::STC:    return 0xF9;
        case Op::CLD:    return 0xFC;
        case Op::STD:    return 0xFD;
        case Op::CDQ:    return 0x99;
        case Op::LEAVE:  return 0xC9;
        case Op::PUSHAD: return 0x60;
        case Op::POPAD:  return 0x61;
        case Op::PUSHFD: return 0x9C;
        case Op::POPFD:  return 0x9D;
        case Op::CMC:    return 0xF5;
        case Op::CLI:    return 0xFA;
        case Op::STI:    return 0xFB;
        case Op::SAHF:   return 0x9E;
        case Op::LAHF:   return 0x9F;
        case Op::XLAT:   return 0xD7;
        case Op::WAIT:   return 0x9B;
        case Op::INT3:   return 0xCC;
        case Op::INTO:   return 0xCE;
        case Op::IRET:   return 0xCF;
        default:         return 0x90;   // not reachable: callers pre-filter
    }
}

// /digit of the F7 group (NOT/NEG/MUL/DIV/IDIV r/m32).
uint8_t unaryDigit(Op op) {
    switch (op) {
        case Op::NOT:  return 2;
        case Op::NEG:  return 3;
        case Op::MUL:  return 4;
        case Op::DIV:  return 6;
        default:       return 7;        // IDIV
    }
}

// /digit of the C0/C1/D2/D3 group (shift / rotate).
uint8_t shiftDigit(Op op) {
    switch (op) {
        case Op::ROL: return 0;
        case Op::ROR: return 1;
        case Op::RCL: return 2;
        case Op::RCR: return 3;
        case Op::SHL: return 4;
        case Op::SHR: return 5;
        default:      return 7;         // SAR
    }
}

// Second opcode byte of a near conditional jump (0F 8x rel32).
uint8_t jccOpcode(Op op) {
    switch (op) {
        case Op::JO:  return 0x80;
        case Op::JNO: return 0x81;
        case Op::JB:  return 0x82;
        case Op::JAE: return 0x83;
        case Op::JBE: return 0x86;
        case Op::JA:  return 0x87;
        case Op::JS:  return 0x88;
        case Op::JNS: return 0x89;
        case Op::JP:  return 0x8A;
        case Op::JNP: return 0x8B;
        case Op::JL:  return 0x8C;
        case Op::JGE: return 0x8D;
        case Op::JLE: return 0x8E;
        default:      return 0x8F;      // JG
    }
}

// ---- one instruction -----------------------------------------------------------
Status encodeOne(const Instruction& in, std::vector<uint8_t>& out,
                 std::vector<Fixup>& fixups, size_t idx) {
    if (in.operandCount > 3) return Status::BAD_OPERANDS;
    const unsigned n = in.operandCount;
    const Operand& a = in.operands[0];
    const Operand& b = in.operands[1];
    const Operand& c = in.operands[2];
    unsigned S = 0;                                         // operation width in bits

    if (in.prefix) put8(out, in.prefix);                    // LOCK / REP / REPNE

    switch (in.op) {
        case Op::NOP:
            if (n != 0) return Status::BAD_OPERANDS;
            put8(out, 0x90);
            return Status::OK;

        case Op::RET:
            if (n == 0) { put8(out, 0xC3); return Status::OK; }
            if (n == 1 && isImm(a)) {                       // RET imm16   C2 iw
                if (a.imm < 0 || a.imm > 65535) return Status::BAD_IMMEDIATE;
                put8(out, 0xC2);
                put16(out, static_cast<uint32_t>(a.imm));
                return Status::OK;
            }
            return Status::BAD_OPERANDS;

        case Op::PUSH:
        case Op::POP: {
            if (n != 1) return Status::BAD_OPERANDS;
            if (isReg(a)) {                                 // 50+rd / 58+rd   (16-bit: 66 prefix)
                S = regBits(a);
                if (S == 8 || !validSize(S)) return Status::BAD_SIZE;
                prefix66(out, S);
                put8(out, (in.op == Op::PUSH ? 0x50u : 0x58u) + code(a.reg));
                return Status::OK;
            }
            if (in.op == Op::PUSH && isImm(a)) {            // PUSH imm8 (6A) / imm32 (68)
                if (fitsInt8(a.imm)) { put8(out, 0x6A); put8(out, static_cast<uint32_t>(a.imm)); }
                else                 { put8(out, 0x68); put32(out, static_cast<uint32_t>(a.imm)); }
                return Status::OK;
            }
            if (isMem(a)) {                                 // PUSH r/m  FF /6     POP r/m  8F /0
                S = a.size ? a.size : 32u;                  // default: dword
                if (S == 8 || !validSize(S)) return Status::BAD_SIZE;
                prefix66(out, S);
                put8(out, in.op == Op::PUSH ? 0xFFu : 0x8Fu);
                return emitModRM(out, in.op == Op::PUSH ? 6u : 0u, a, fixups, idx);
            }
            return Status::BAD_OPERANDS;
        }

        case Op::INC:
        case Op::DEC: {
            if (n != 1 || !isRM(a)) return Status::BAD_OPERANDS;
            if (!needSize(&a, 1, S)) return Status::BAD_SIZE;
            if (isReg(a) && S != 8) {                       // 40+rd / 48+rd  (16-bit: 66 prefix)
                prefix66(out, S);
                put8(out, (in.op == Op::INC ? 0x40u : 0x48u) + code(a.reg));
                return Status::OK;
            }
            prefix66(out, S);                               // FE /d (8-bit)   FF /d (16/32-bit)
            put8(out, S == 8 ? 0xFEu : 0xFFu);
            return emitModRM(out, in.op == Op::INC ? 0u : 1u, a, fixups, idx);
        }

        case Op::JMP:
        case Op::CALL:
        case Op::JE:
        case Op::JNE: {
            if (n == 1 && (in.op == Op::JMP || in.op == Op::CALL) && isRM(a)) {
                // Indirect near jump/call through a register or memory:  FF /4  FF /2   (32-bit only)
                const unsigned w = isReg(a) ? regBits(a) : (a.size ? a.size : 32u);
                if (w != 32) return Status::BAD_SIZE;
                put8(out, 0xFF);
                return emitModRM(out, in.op == Op::JMP ? 4u : 2u, a, fixups, idx);
            }
            if (n != 1 || a.kind != OperandKind::LABEL) return Status::BAD_OPERANDS;
            if (in.op == Op::JMP) {
                put8(out, 0xE9);                            // JMP  rel32
            } else if (in.op == Op::CALL) {
                put8(out, 0xE8);                            // CALL rel32
            } else {
                put8(out, 0x0F);                            // Jcc  rel32
                put8(out, in.op == Op::JE ? 0x84u : 0x85u);
            }
            Fixup f = { out.size(), a.label, idx, FX_REL32, 0 };
            fixups.push_back(f);
            put32(out, 0);                                  // rel32 placeholder
            return Status::OK;
        }

        case Op::MOV: {
            if (n != 2) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S)) return Status::BAD_SIZE;
            if (isImm(b) && !immFits(b.imm, S)) return Status::BAD_IMMEDIATE;
            if (isReg(a) && isImm(b)) {                     // MOV r, imm   B0+rb / B8+rd (66 B8+rw)
                prefix66(out, S);
                put8(out, (S == 8 ? 0xB0u : 0xB8u) + code(a.reg));
                putImm(out, immNorm(b.imm, S), S);
                return Status::OK;
            }
            if (isReg(a) && isRM(b)) {                      // MOV r, r/m   8A /r | 8B /r
                prefix66(out, S);
                put8(out, S == 8 ? 0x8Au : 0x8Bu);
                return emitModRM(out, code(a.reg), b, fixups, idx);
            }
            if (isMem(a) && isReg(b)) {                     // MOV r/m, r   88 /r | 89 /r
                prefix66(out, S);
                put8(out, S == 8 ? 0x88u : 0x89u);
                return emitModRM(out, code(b.reg), a, fixups, idx);
            }
            if (isMem(a) && isImm(b)) {                     // MOV r/m, imm C6 /0 ib | C7 /0 iw/id
                prefix66(out, S);
                put8(out, S == 8 ? 0xC6u : 0xC7u);
                Status s = emitModRM(out, 0, a, fixups, idx);
                if (s != Status::OK) return s;
                putImm(out, immNorm(b.imm, S), S);
                return Status::OK;
            }
            return Status::BAD_OPERANDS;
        }

        case Op::LEA: {                                     // LEA r16/r32, m     8D /r
            if (n != 2 || !isReg(a) || !isMem(b)) return Status::BAD_OPERANDS;
            S = regBits(a);
            if (S == 8 || !validSize(S)) return Status::BAD_SIZE;
            prefix66(out, S);
            put8(out, 0x8D);
            return emitModRM(out, code(a.reg), b, fixups, idx);
        }

        case Op::MOVZX:
        case Op::MOVSX: {                                   // 0F B6/B7 (zero)   0F BE/BF (sign)
            if (n != 2 || !isReg(a) || !isRM(b)) return Status::BAD_OPERANDS;
            const unsigned D = regBits(a);
            const unsigned s = isReg(b) ? regBits(b) : b.size;   // source width
            if (D == 8 || !validSize(D) || (s != 8 && s != 16) || s >= D) return Status::BAD_SIZE;
            prefix66(out, D);
            put8(out, 0x0F);
            const bool zx = (in.op == Op::MOVZX);
            put8(out, zx ? (s == 8 ? 0xB6u : 0xB7u) : (s == 8 ? 0xBEu : 0xBFu));
            return emitModRM(out, code(a.reg), b, fixups, idx);
        }

        // ===== added instruction set ===========================================
        case Op::HLT:
        case Op::CLC:
        case Op::STC:
        case Op::CLD:
        case Op::STD:
        case Op::CDQ:
        case Op::LEAVE:
        case Op::PUSHAD:
        case Op::POPAD:
        case Op::PUSHFD:
        case Op::POPFD:
        case Op::CMC:
        case Op::CLI:
        case Op::STI:
        case Op::SAHF:
        case Op::LAHF:
        case Op::XLAT:
        case Op::WAIT:
        case Op::INT3:
        case Op::INTO:
        case Op::IRET:
            if (n != 0) return Status::BAD_OPERANDS;
            put8(out, singleByteOpcode(in.op));
            return Status::OK;

        case Op::CBW:                                       // 66 98   AL -> AX
        case Op::CWD:                                       // 66 99   AX -> DX:AX
            if (n != 0) return Status::BAD_OPERANDS;
            put8(out, 0x66);
            put8(out, in.op == Op::CBW ? 0x98u : 0x99u);
            return Status::OK;
        case Op::CWDE:                                      // 98      AX -> EAX
            if (n != 0) return Status::BAD_OPERANDS;
            put8(out, 0x98);
            return Status::OK;

        case Op::UD2:   case Op::CPUID:  case Op::RDTSC:
            if (n != 0) return Status::BAD_OPERANDS;
            put8(out, 0x0F);
            put8(out, in.op == Op::UD2 ? 0x0Bu : (in.op == Op::CPUID ? 0xA2u : 0x31u));
            return Status::OK;

        case Op::MOVS: case Op::CMPS: case Op::STOS: case Op::LODS: case Op::SCAS: {
            if (n != 0) return Status::BAD_OPERANDS;
            if (!validSize(in.size)) return Status::BAD_SIZE;
            uint8_t opc = 0;
            switch (in.op) {
                case Op::MOVS: opc = 0xA4; break;
                case Op::CMPS: opc = 0xA6; break;
                case Op::STOS: opc = 0xAA; break;
                case Op::LODS: opc = 0xAC; break;
                default:       opc = 0xAE; break;           // SCAS
            }
            prefix66(out, in.size);
            put8(out, in.size == 8 ? opc : static_cast<uint8_t>(opc + 1));
            return Status::OK;
        }

        case Op::NOT:
        case Op::NEG:
        case Op::MUL:
        case Op::DIV:
        case Op::IDIV: {
            if (n != 1 || !isRM(a)) return Status::BAD_OPERANDS;
            if (!needSize(&a, 1, S)) return Status::BAD_SIZE;
            prefix66(out, S);
            put8(out, S == 8 ? 0xF6u : 0xF7u);              // F6/F7 /digit
            return emitModRM(out, unaryDigit(in.op), a, fixups, idx);
        }

        case Op::SHL:
        case Op::SHR:
        case Op::SAR:
        case Op::ROL:
        case Op::ROR:
        case Op::RCL:
        case Op::RCR: {
            if (n != 2 || !isRM(a)) return Status::BAD_OPERANDS;
            if (!needSize(&a, 1, S)) return Status::BAD_SIZE;
            if (isImm(b) && b.imm == 1) {                   // r/m, 1      D0 /d | D1 /d   (no immediate byte)
                prefix66(out, S);
                put8(out, S == 8 ? 0xD0u : 0xD1u);
                return emitModRM(out, shiftDigit(in.op), a, fixups, idx);
            }
            if (isImm(b)) {                                 // r/m, imm8   C0 /d ib | C1 /d ib
                if (b.imm < 0 || b.imm > 31) return Status::BAD_IMMEDIATE;
                prefix66(out, S);
                put8(out, S == 8 ? 0xC0u : 0xC1u);
                Status s = emitModRM(out, shiftDigit(in.op), a, fixups, idx);
                if (s != Status::OK) return s;
                put8(out, static_cast<uint32_t>(b.imm));
                return Status::OK;
            }
            if (isCL(b)) {                                  // r/m, CL     D2 /d | D3 /d
                prefix66(out, S);
                put8(out, S == 8 ? 0xD2u : 0xD3u);
                return emitModRM(out, shiftDigit(in.op), a, fixups, idx);
            }
            return Status::BAD_OPERANDS;
        }

        case Op::INT:
            if (n != 1 || !isImm(a)) return Status::BAD_OPERANDS;
            if (a.imm < 0 || a.imm > 255) return Status::BAD_IMMEDIATE;
            put8(out, 0xCD);                                // CD ib
            put8(out, static_cast<uint32_t>(a.imm));
            return Status::OK;

        case Op::XCHG:
            if (n != 2) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S)) return Status::BAD_SIZE;
            if (S != 8 && isReg(a) && isReg(b) && (isAcc(a) || isAcc(b))) {
                // XCHG AX/EAX, r   90+rd  (1 byte; 66 prefix for 16-bit).  Operands commute.
                prefix66(out, S);
                put8(out, 0x90u + code(isAcc(a) ? b.reg : a.reg));
                return Status::OK;
            }
            if (isRM(a) && isReg(b)) {                      // XCHG r/m, r   86 /r | 87 /r
                prefix66(out, S);
                put8(out, S == 8 ? 0x86u : 0x87u);
                return emitModRM(out, code(b.reg), a, fixups, idx);
            }
            if (isReg(a) && isMem(b)) {                     // XCHG r, m  (operands commute)
                prefix66(out, S);
                put8(out, S == 8 ? 0x86u : 0x87u);
                return emitModRM(out, code(a.reg), b, fixups, idx);
            }
            return Status::BAD_OPERANDS;

        case Op::IMUL:
            if (n == 1 && isRM(a)) {                        // IMUL r/m        F6 /5 | F7 /5
                if (!needSize(&a, 1, S)) return Status::BAD_SIZE;
                prefix66(out, S);
                put8(out, S == 8 ? 0xF6u : 0xF7u);
                return emitModRM(out, 5, a, fixups, idx);
            }
            if (n == 2 && isReg(a) && isRM(b)) {            // IMUL r, r/m     0F AF /r   (16/32-bit)
                if (!needSize(in.operands, 2, S) || S == 8) return Status::BAD_SIZE;
                prefix66(out, S);
                put8(out, 0x0F);
                put8(out, 0xAF);
                return emitModRM(out, code(a.reg), b, fixups, idx);
            }
            if (n == 3 && isReg(a) && isRM(b) && isImm(c)) { // IMUL r, r/m, imm   6B /r ib | 69 /r iw/id
                if (!needSize(in.operands, 2, S) || S == 8) return Status::BAD_SIZE;
                if (!immFits(c.imm, S)) return Status::BAD_IMMEDIATE;
                const int32_t v = immNorm(c.imm, S);
                const bool short8 = fitsInt8(v);
                prefix66(out, S);
                put8(out, short8 ? 0x6Bu : 0x69u);
                Status s = emitModRM(out, code(a.reg), b, fixups, idx);
                if (s != Status::OK) return s;
                if (short8) put8(out, static_cast<uint32_t>(v));
                else        putImm(out, v, S);
                return Status::OK;
            }
            return Status::BAD_OPERANDS;

        case Op::TEST: {
            if (n != 2) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S)) return Status::BAD_SIZE;
            if (isRM(a) && isImm(b)) {
                if (!immFits(b.imm, S)) return Status::BAD_IMMEDIATE;
                const int32_t v = immNorm(b.imm, S);
                if (isAcc(a)) {                             // TEST AL/AX/EAX, imm   A8 ib | A9 iw/id
                    prefix66(out, S);
                    put8(out, S == 8 ? 0xA8u : 0xA9u);
                    putImm(out, v, S);
                    return Status::OK;
                }
                prefix66(out, S);                           // TEST r/m, imm   F6 /0 ib | F7 /0 iw/id
                put8(out, S == 8 ? 0xF6u : 0xF7u);
                Status s = emitModRM(out, 0, a, fixups, idx);
                if (s != Status::OK) return s;
                putImm(out, v, S);
                return Status::OK;
            }
            if (isRM(a) && isReg(b)) {                      // TEST r/m, r   84 /r | 85 /r
                prefix66(out, S);
                put8(out, S == 8 ? 0x84u : 0x85u);
                return emitModRM(out, code(b.reg), a, fixups, idx);
            }
            if (isReg(a) && isMem(b)) {                     // TEST r, m  (operands commute)
                prefix66(out, S);
                put8(out, S == 8 ? 0x84u : 0x85u);
                return emitModRM(out, code(a.reg), b, fixups, idx);
            }
            return Status::BAD_OPERANDS;
        }

        case Op::SETCC: {                                   // SETcc r/m8   0F 90+cc /0
            if (n != 1 || !isRM(a) || in.cond > 15) return Status::BAD_OPERANDS;
            if (isReg(a) ? regBits(a) != 8 : (a.size != 0 && a.size != 8)) return Status::BAD_SIZE;
            put8(out, 0x0F);
            put8(out, 0x90u + in.cond);
            return emitModRM(out, 0, a, fixups, idx);
        }

        case Op::CMOVCC: {                                  // CMOVcc r, r/m   0F 40+cc /r   (16/32-bit)
            if (n != 2 || !isReg(a) || !isRM(b) || in.cond > 15) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S) || S == 8) return Status::BAD_SIZE;
            prefix66(out, S);
            put8(out, 0x0F);
            put8(out, 0x40u + in.cond);
            return emitModRM(out, code(a.reg), b, fixups, idx);
        }

        case Op::BT:
        case Op::BTS:
        case Op::BTR:
        case Op::BTC: {
            if (n != 2 || !isRM(a)) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S) || S == 8) return Status::BAD_SIZE;
            const unsigned k = (in.op == Op::BT) ? 0 : (in.op == Op::BTS) ? 1 : (in.op == Op::BTR) ? 2 : 3;
            if (isReg(b)) {                                 // BT* r/m, r    0F A3 / AB / B3 / BB /r
                prefix66(out, S);
                put8(out, 0x0F);
                put8(out, 0xA3u + 8u * k);
                return emitModRM(out, code(b.reg), a, fixups, idx);
            }
            if (isImm(b)) {                                 // BT* r/m, imm8  0F BA /4..7 ib
                if (b.imm < 0 || b.imm > 255) return Status::BAD_IMMEDIATE;
                prefix66(out, S);
                put8(out, 0x0F);
                put8(out, 0xBA);
                Status s = emitModRM(out, 4u + k, a, fixups, idx);
                if (s != Status::OK) return s;
                put8(out, static_cast<uint32_t>(b.imm));
                return Status::OK;
            }
            return Status::BAD_OPERANDS;
        }

        case Op::BSF:
        case Op::BSR:                                       // BSF/BSR r, r/m   0F BC / BD /r
            if (n != 2 || !isReg(a) || !isRM(b)) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S) || S == 8) return Status::BAD_SIZE;
            prefix66(out, S);
            put8(out, 0x0F);
            put8(out, in.op == Op::BSF ? 0xBCu : 0xBDu);
            return emitModRM(out, code(a.reg), b, fixups, idx);

        case Op::BSWAP:                                     // BSWAP r32   0F C8+rd
            if (n != 1 || !isReg(a)) return Status::BAD_OPERANDS;
            if (regBits(a) != 32) return Status::BAD_SIZE;
            put8(out, 0x0F);
            put8(out, 0xC8u + code(a.reg));
            return Status::OK;

        case Op::XADD:                                      // XADD r/m, r    0F C0 | C1 /r
        case Op::CMPXCHG: {                                 // CMPXCHG r/m, r 0F B0 | B1 /r
            if (n != 2 || !isRM(a) || !isReg(b)) return Status::BAD_OPERANDS;
            if (!needSize(in.operands, 2, S)) return Status::BAD_SIZE;
            prefix66(out, S);
            put8(out, 0x0F);
            const uint8_t opc = (in.op == Op::XADD) ? 0xC0 : 0xB0;
            put8(out, S == 8 ? opc : static_cast<uint8_t>(opc + 1));
            return emitModRM(out, code(b.reg), a, fixups, idx);
        }

        case Op::LOOP:
        case Op::LOOPE:
        case Op::LOOPNE:
        case Op::JECXZ: {                                   // rel8 only: E2 / E1 / E0 / E3
            if (n != 1 || a.kind != OperandKind::LABEL) return Status::BAD_OPERANDS;
            put8(out, in.op == Op::LOOP ? 0xE2u : in.op == Op::LOOPE ? 0xE1u
                    : in.op == Op::LOOPNE ? 0xE0u : 0xE3u);
            Fixup f = { out.size(), a.label, idx, FX_REL8, 0 };
            fixups.push_back(f);
            put8(out, 0);                                   // rel8 placeholder
            return Status::OK;
        }

        case Op::JL:
        case Op::JGE:
        case Op::JLE:
        case Op::JG:
        case Op::JB:
        case Op::JAE:
        case Op::JBE:
        case Op::JA:
        case Op::JS:
        case Op::JNS:
        case Op::JO:
        case Op::JNO:
        case Op::JP:
        case Op::JNP: {
            if (n != 1 || a.kind != OperandKind::LABEL) return Status::BAD_OPERANDS;
            put8(out, 0x0F);                                // Jcc rel32   0F 8x id
            put8(out, jccOpcode(in.op));
            Fixup f = { out.size(), a.label, idx, FX_REL32, 0 };
            fixups.push_back(f);
            put32(out, 0);                                  // rel32 placeholder
            return Status::OK;
        }
        // =======================================================================

        default:
            break;                                          // ALU group below
    }

    uint8_t rmR, rRm, ext;
    if (!aluForm(in.op, rmR, rRm, ext)) return Status::BAD_OPCODE;
    if (n != 2) return Status::BAD_OPERANDS;
    if (!needSize(in.operands, 2, S)) return Status::BAD_SIZE;

    if (isRM(a) && isImm(b)) {
        if (!immFits(b.imm, S)) return Status::BAD_IMMEDIATE;
        const int32_t v = immNorm(b.imm, S);
        if (S == 8) {
            if (isAcc(a)) {                                 // op AL, imm8      (opcode+3)  2 bytes
                put8(out, rmR + 3u);
                put8(out, static_cast<uint32_t>(v));
                return Status::OK;
            }
            put8(out, 0x80);                                // op r/m8, imm8    80 /d ib
            Status s = emitModRM(out, ext, a, fixups, idx);
            if (s != Status::OK) return s;
            put8(out, static_cast<uint32_t>(v));
            return Status::OK;
        }
        const bool short8 = fitsInt8(v);
        // op AX/EAX, imm  (opcode+4, no ModRM) -- the short form that Worklet 1's
        // Pass 1 assumes whenever the immediate doesn't fit in 8 bits.
        if (!short8 && isAcc(a)) {
            prefix66(out, S);
            put8(out, rmR + 4u);
            putImm(out, v, S);
            return Status::OK;
        }
        prefix66(out, S);
        put8(out, short8 ? 0x83u : 0x81u);                  // 83 /d ib  or  81 /d iw|id
        Status s = emitModRM(out, ext, a, fixups, idx);
        if (s != Status::OK) return s;
        if (short8) put8(out, static_cast<uint32_t>(v));
        else        putImm(out, v, S);
        return Status::OK;
    }
    if (isReg(a) && isRM(b)) {                              // op r, r/m   /r
        prefix66(out, S);
        put8(out, S == 8 ? rRm - 1u : rRm);
        return emitModRM(out, code(a.reg), b, fixups, idx);
    }
    if (isMem(a) && isReg(b)) {                             // op r/m, r   /r
        prefix66(out, S);
        put8(out, S == 8 ? rmR - 1u : rmR);
        return emitModRM(out, code(b.reg), a, fixups, idx);
    }
    return Status::BAD_OPERANDS;
}

Result fail(Status s, size_t idx) {
    Result r = { s, idx };
    return r;
}

Result encodeImpl(const Program& prog, std::vector<uint8_t>& out,
                  std::vector<size_t>* offs) {
    out.reserve(prog.count * 8);
    if (offs) offs->reserve(prog.count + 1);

    std::vector<size_t> labelOffset(prog.labelCount, kUndefined);   // O(1) lookup by label ID
    std::vector<Fixup>  fixups;

    // Single pass: emit bytes, record where each label lands.
    for (size_t i = 0; i < prog.count; ++i) {
        const Instruction& in = prog.instrs[i];

        if (in.defLabel != NO_LABEL) {
            if (in.defLabel >= prog.labelCount)          return fail(Status::LABEL_OUT_OF_RANGE, i);
            if (labelOffset[in.defLabel] != kUndefined)  return fail(Status::DUPLICATE_LABEL, i);
            labelOffset[in.defLabel] = out.size();
        }
        if (offs) offs->push_back(out.size());

        Status s = encodeOne(in, out, fixups, i);
        if (s != Status::OK) return fail(s, i);
    }
    if (offs) offs->push_back(out.size());                  // total size

    // A label that sits after the last instruction.
    if (prog.endLabel != NO_LABEL) {
        if (prog.endLabel >= prog.labelCount)            return fail(Status::LABEL_OUT_OF_RANGE, prog.count);
        if (labelOffset[prog.endLabel] != kUndefined)    return fail(Status::DUPLICATE_LABEL, prog.count);
        labelOffset[prog.endLabel] = out.size();
    }

    // Back-patch label references.
    //   relative: rel32 = target - (address of the byte after the rel32 field)  (rel8 likewise)
    //   absolute: baseAddress + target + addend
    if (prog.count > 2) {
        std::cout << "[Worklet 2: encodeImpl] Emitted " << out.size()
                  << " machine code bytes. Resolving " << fixups.size() << " label fixup(s)...\n";
    }

    for (size_t k = 0; k < fixups.size(); ++k) {
        const Fixup& f = fixups[k];
        if (f.label >= prog.labelCount)        return fail(Status::LABEL_OUT_OF_RANGE, f.instr);
        const size_t target = labelOffset[f.label];
        if (target == kUndefined)              return fail(Status::UNDEFINED_LABEL, f.instr);

        uint32_t v;
        if (f.kind == FX_ABS32) {
            v = prog.baseAddress + static_cast<uint32_t>(target) + static_cast<uint32_t>(f.addend);
        } else if (f.kind == FX_REL8) {
            const int64_t rel = static_cast<int64_t>(target) - static_cast<int64_t>(f.pos + 1);
            if (rel < -128 || rel > 127) return fail(Status::BRANCH_RANGE, f.instr);
            out[f.pos] = static_cast<uint8_t>(rel);
            continue;
        } else {
            const int64_t rel = static_cast<int64_t>(target) - static_cast<int64_t>(f.pos + 4);
            v = static_cast<uint32_t>(rel);                 // two's-complement wrap is intended
        }
        patch32(out, f.pos, v);
    }

    if (prog.count > 2 && !fixups.empty()) {
        std::cout << "[Worklet 2: encodeImpl] Backpatching complete: all label offsets resolved successfully.\n";
    }

    return fail(Status::OK, 0);
}

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
        case Status::BAD_IMMEDIATE:      return "immediate out of range for this instruction";
        case Status::BAD_SIZE:           return "operand size missing, conflicting or not allowed for this instruction";
        case Status::BRANCH_RANGE:       return "short branch target out of range (-128..127)";
    }
    return "unknown status";
}

Result encode(const Program& prog, std::vector<uint8_t>& out, std::vector<size_t>* instrOffsets) {
    out.clear();
    if (instrOffsets) instrOffsets->clear();

    if (prog.count > 2) {
        std::cout << "[Worklet 2: encode] Starting single-pass encoding for " << prog.count
                  << " instructions (" << prog.labelCount << " labels)...\n";
    }

    Result r = encodeImpl(prog, out, instrOffsets);
    if (r.status != Status::OK) {
        if (prog.count > 2) {
            std::cout << "[Worklet 2: encode] Encoding error at instruction " << r.instrIndex
                      << ": " << statusText(r.status) << "\n";
        }
        out.clear();
        if (instrOffsets) instrOffsets->clear();
    } else {
        if (prog.count > 2) {
            std::cout << "[Worklet 2: encode] Encoding completed successfully: " << out.size()
                      << " machine code bytes generated.\n";
        }
    }
    return r;
}

bool writeBinFile(const char* path, const std::vector<uint8_t>& bytes) {
    std::cout << "[Worklet 2: writeBinFile] Writing raw binary file: " << path
              << " (" << bytes.size() << " bytes)...\n";
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!bytes.empty()) {
        f.write(reinterpret_cast<const char*>(&bytes[0]), static_cast<std::streamsize>(bytes.size()));
    }
    f.close();
    return !f.fail();
}

} // namespace w2

#ifdef WORKLET2_STANDALONE
int main(int argc, char* argv[]) {
    std::cout << "=== Worklet 2 Standalone Runner ===\n";
    w2::Instruction instrs[4];
    instrs[0] = w2::inst2(w2::Op::MOV, w2::regOp(w2::Reg::ECX, 32), w2::immOp(5));
    instrs[1] = w2::inst2(w2::Op::MOV, w2::regOp(w2::Reg::ESI, 32), w2::immOp(0));
    instrs[2] = w2::inst2(w2::Op::XOR, w2::regOp(w2::Reg::EAX, 32), w2::regOp(w2::Reg::EAX, 32));
    instrs[3] = w2::inst0(w2::Op::RET);
    w2::Program prog = w2::makeProgram(instrs, 4, 1, 0, 0);

    std::vector<uint8_t> bytes;
    std::vector<size_t> offs;
    w2::Result r = w2::encode(prog, bytes, &offs);
    if (r.status != w2::Status::OK) {
        std::cerr << "Worklet 2 encoding failed: " << w2::statusText(r.status) << "\n";
        return 1;
    }
    const char* outPath = (argc > 1) ? argv[1] : "w2_output.bin";
    w2::writeBinFile(outPath, bytes);
    std::cout << "Successfully encoded " << bytes.size() << " bytes to " << outPath << "\n";
    return 0;
}
#endif 
