#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace w2 {

// ---- Strongly typed vocabulary ------------------------------------------

// Numbered exactly as the hardware encodes them in ModRM/SIB fields.
enum class Reg : uint8_t {
    EAX = 0, ECX = 1, EDX = 2, EBX = 3, ESP = 4, EBP = 5, ESI = 6, EDI = 7,
    NONE = 0xFF
};

enum class Op : uint8_t {
    MOV, ADD, SUB, AND, OR, XOR, CMP,   // two operands
    PUSH, POP, INC, DEC,                // one register operand
    JMP, CALL, JE, JNE,                 // one LABEL operand (rel32, near form)
    RET, NOP,                           // no operands

    // ---- added instruction set -------------------------------------------
    TEST, XCHG, IMUL,                   // two operands (IMUL = r32, r/m32 form)
    SHL, SHR, SAR,                      // r/m32, imm8 (count 0..31)
    NOT, NEG, MUL, DIV, IDIV,           // one register operand (F7 group)
    INT,                                // one imm8 operand
    JL, JGE, JLE, JG,                   // signed conditional jumps  (rel32, near)
    JB, JAE, JBE, JA,                   // unsigned conditional jumps (rel32, near)
    JS, JNS,                            // sign-flag conditional jumps (rel32, near)
    HLT, CLC, STC, CLD, STD, CDQ,       // no operands, single byte
    LEAVE, PUSHAD, POPAD, PUSHFD, POPFD, // no operands, single byte

    // ---- added: 8/16-bit forms, memory operands, rest of the integer set ----
    // (every two-operand / one-operand instruction above also accepts 8-, 16-
    //  and 32-bit operands and memory operands; see Operand::size)
    ADC, SBB,                           // ALU group, like ADD/SUB
    ROL, ROR, RCL, RCR,                 // rotates, like SHL/SHR/SAR (imm8 or CL count)
    LEA,                                // r16/r32, m
    MOVZX, MOVSX,                       // r16/r32, r/m8|r/m16
    SETCC, CMOVCC,                      // condition code in Instruction::cond
    BT, BTS, BTR, BTC, BSF, BSR,        // bit instructions (16/32-bit)
    BSWAP, XADD, CMPXCHG,
    CBW, CWDE, CWD,                     // sign extension of the accumulator
    CMC, CLI, STI, SAHF, LAHF, XLAT, WAIT, // no operands
    INT3, INTO, IRET, UD2, CPUID, RDTSC,
    MOVS, CMPS, SCAS, LODS, STOS,       // string ops; width in Instruction::size
    LOOP, LOOPE, LOOPNE, JECXZ,         // one LABEL operand, rel8 (must be within -128..127)
    JO, JNO, JP, JNP                    // remaining conditional jumps (rel32, near)
};

// MEMLABEL = absolute memory operand "[label]"; encoded as [disp32] where
// disp32 = Program::baseAddress + offset of the label.
enum class OperandKind : uint8_t { NONE, REG, IMM, MEM, LABEL, MEMLABEL };

typedef uint32_t LabelId;
constexpr LabelId NO_LABEL = 0xFFFFFFFFu;

// [base + index*scale + disp].  Absent parts are Reg::NONE; with neither
// base nor index, `disp` is an absolute address.
//
// `label` (NO_LABEL if unused) adds the address of a label to the displacement:
//     [base + index*scale + label + disp]
// With a label the displacement is always emitted as a 32-bit field, because
// its value is only known after back-patching.  Use memOp()/memLabelOp() so
// that `label` is initialised to NO_LABEL.
struct MemOperand {
    Reg     base;
    Reg     index;   // ESP is not encodable as an index
    uint8_t scale;   // 1, 2, 4 or 8 (ignored when index == NONE)
    int32_t disp;
    LabelId label;   // NO_LABEL = none
};

// Operand::size is in BITS (8, 16 or 32):
//   REG : register width.  The register number is the hardware number, so with
//         size 8 the codes 0..7 mean AL CL DL BL AH CH DH BH, with size 16
//         AX CX DX BX SP BP SI DI, with size 32 EAX ECX EDX EBX ESP EBP ESI EDI.
//         (size 0 is treated as 32.)
//   MEM / MEMLABEL : the BYTE/WORD/DWORD keyword; 0 = not stated.  A memory
//         operand with no register operand to take its size from needs it.
//   IMM / LABEL : unused.
struct Operand {
    OperandKind kind;
    uint8_t     size;
    union {
        Reg        reg;     // kind == REG
        int32_t    imm;     // kind == IMM
        LabelId    label;   // kind == LABEL or MEMLABEL
        MemOperand mem;     // kind == MEM
    };
};

struct Instruction {
     Op      op;
    uint8_t  operandCount;   // 0..3
    uint8_t  size;           // width in bits of the implicit operand (MOVS/CMPS/SCAS/LODS/STOS), else 0
    uint8_t  prefix;         // 0, or one raw prefix byte: 0xF0 LOCK, 0xF2 REPNE, 0xF3 REP/REPE
    uint8_t  cond;           // condition code 0..15 for SETCC / CMOVCC (0=O 1=NO 2=B 3=AE 4=E 5=NE 6=BE 7=A
                             //   8=S 9=NS 10=P 11=NP 12=L 13=GE 14=LE 15=G)
    Operand  operands[3];    // operands[0] is the destination (Intel order)
    LabelId  defLabel;       // label defined at this instruction, or NO_LABEL
};

static_assert(std::is_pod<Instruction>::value, "Instruction must stay plain-old-data");

struct Program {
    const Instruction* instrs;
    size_t             count;
    uint32_t           labelCount;    // valid label IDs are [0, labelCount)
    uint32_t           baseAddress;   // added to label offsets for absolute ([label]) operands
    LabelId            endLabel;      // label located after the last instruction, or NO_LABEL
};

static_assert(std::is_pod<Program>::value, "Program must stay plain-old-data");

// ---- Convenience builders (pure struct filling) ----------------------------

inline Operand regOp(Reg r, uint8_t bits = 32) {
    Operand o = Operand(); o.kind = OperandKind::REG; o.size = bits; o.reg = r; return o;
}
inline Operand immOp(int32_t v) {
    Operand o = Operand(); o.kind = OperandKind::IMM; o.imm = v; return o;
}
inline Operand labelOp(LabelId id) {
    Operand o = Operand(); o.kind = OperandKind::LABEL; o.label = id; return o;
}
inline Operand labelMemOp(LabelId id) {
    Operand o = Operand(); o.kind = OperandKind::MEMLABEL; o.label = id; return o;
}
inline Operand memOp(Reg base, Reg index, uint8_t scale, int32_t disp, uint8_t bits = 0) {
    Operand o = Operand(); o.kind = OperandKind::MEM; o.size = bits;
    o.mem.base = base; o.mem.index = index; o.mem.scale = scale; o.mem.disp = disp;
    o.mem.label = NO_LABEL;
    return o;
}
// [base + index*scale + label + disp]
inline Operand memLabelOp(Reg base, Reg index, uint8_t scale, int32_t disp, LabelId label,
                          uint8_t bits = 0) {
    Operand o = memOp(base, index, scale, disp, bits); o.mem.label = label; return o;
}
// Same operand with a BYTE/WORD/DWORD size attached.
inline Operand sized(Operand o, uint8_t bits) { o.size = bits; return o; }

inline Instruction inst0(Op op, LabelId def = NO_LABEL) {
    Instruction i = Instruction(); i.op = op; i.operandCount = 0; i.defLabel = def; return i;
}
inline Instruction inst1(Op op, Operand a, LabelId def = NO_LABEL) {
    Instruction i = Instruction(); i.op = op; i.operandCount = 1; i.operands[0] = a; i.defLabel = def; return i;
}
inline Instruction inst2(Op op, Operand a, Operand b, LabelId def = NO_LABEL) {
    Instruction i = Instruction(); i.op = op; i.operandCount = 2;
    i.operands[0] = a; i.operands[1] = b; i.defLabel = def; return i;
}

inline Instruction inst3(Op op, Operand a, Operand b, Operand c, LabelId def = NO_LABEL) {
    Instruction i = Instruction(); i.op = op; i.operandCount = 3;
    i.operands[0] = a; i.operands[1] = b; i.operands[2] = c; i.defLabel = def; return i;
}

inline Program makeProgram(const Instruction* instrs, size_t count, uint32_t labelCount,
                           uint32_t baseAddress = 0, LabelId endLabel = NO_LABEL) {
    Program p = Program();
    p.instrs = instrs; p.count = count; p.labelCount = labelCount;
    p.baseAddress = baseAddress; p.endLabel = endLabel;
    return p;
}

// ---- Encoder API ----------------------------------------------------------

enum class Status : uint8_t {
    OK,
    BAD_OPCODE,          // op value not handled
    BAD_OPERANDS,        // wrong count or unsupported operand combination
    BAD_SCALE,           // memory scale not 1/2/4/8
    BAD_INDEX_REG,       // ESP used as index
    LABEL_OUT_OF_RANGE,  // label ID >= labelCount
    DUPLICATE_LABEL,     // same label ID defined twice
    UNDEFINED_LABEL,     // label referenced but never defined
    BAD_IMMEDIATE,       // immediate out of range for this instruction (shift count, INT number, operand width)
    BAD_SIZE,            // operand size missing (e.g. [mem], imm with no BYTE/WORD/DWORD), conflicting, or not allowed
    BRANCH_RANGE         // rel8 target (LOOP/JECXZ) farther than -128..127 bytes
};

struct Result {
    Status status;
    size_t instrIndex;   // index of the offending instruction (valid when status != OK);
                         // == prog.count when the problem is Program::endLabel
};

// Short human-readable name of a status code.
const char* statusText(Status s);

// Encodes `prog` into raw x86-32 machine code (`out` is cleared first).
// Single pass: each instruction is emitted as it is visited, and label
// references are back-patched once every label offset is known.
//
// If `instrOffsets` is non-null it receives prog.count + 1 entries: the byte
// offset at which each instruction starts, plus the total size as the last
// entry.  Worklet 1's Pass 1 already assigned an address and length to every
// instruction; comparing against these offsets proves both sides agree.
//
// On failure `out` (and `instrOffsets`) are left empty.
Result encode(
    const Program& prog, 
    std::vector<uint8_t>& out,
    std::vector<size_t>* instrOffsets = nullptr
);

// Writes `bytes` verbatim to `path` (binary mode). Returns false on I/O error.
bool writeBinFile(
    const char* path, 
    const std::vector<uint8_t>& bytes
);

} // namespace w2
