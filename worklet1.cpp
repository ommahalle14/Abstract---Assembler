// =====================================================================
//  Worklet 1 : Assembler for an Abstract Machine  (x86-32)
//  ---------------------------------------------------------------------
//  Pipeline:
//      .asm source
//        -> Pooja  : tokenizeLine()        Lexer
//        -> Om     : parseInstruction()    Parser + Instruction IR
//        -> Ved    : pass1()               Location Counter + Symbol Table
//        -> Mayuri : pass2Resolve()        Forward/backward reference
//                                          resolution + error handling
//        -> Worklet1Output (in memory)
//        -> worklet2_receive() / worklet3_receive()   (function-call handoff)
//
//  HANDOFF DESIGN:
//      Worklet 1 does NOT write any output files. runWorklet1() returns a
//      Worklet1Output struct and Worklet 2 / Worklet 3 are called with that
//      struct directly. No disk I/O, no re-tokenising, no re-parsing.
//
//  Worklet 1 does NOT encode machine code and does NOT build an ELF file.
//
//  Supported instructions (8-bit, 16-bit, and 32-bit registers, and memory operands):
//      MOV/ADD/ADC/SUB/SBB/CMP/AND/OR/XOR/TEST  reg|mem, reg|mem|imm
//      LOAD/STORE/LEA                   reg, mem|reg|imm | mem, reg
//      XCHG                             reg|mem, reg|mem
//      IMUL                             reg, reg|mem|imm
//      SHL/SHR/SAR/SAL/ROL/ROR/RCL/RCR  reg|mem, imm|reg
//      INC/DEC/PUSH/POP/NOT/NEG         reg|mem
//      MUL/DIV/IDIV                     reg|mem
//      INT                              imm
//      CDQ/PUSHAD/POPAD/PUSHFD/POPFD/CLC/STC/CMC/CLD/STD/LEAVE/HLT/NOP/RET
//      LODS/LODSB/LODSW/LODSD/STOS/STOSB/STOSW/STOSD/MOVSB/W/D/CMPSB/W/D/SCASB/W/D
//      LAHF/SAHF/CLI/STI/WAIT
//      LOOP/LOOPE/LOOPZ/LOOPNE/LOOPNZ/JECXZ  label (SHORT, rel8: target within -128..+127)
//  Numbers:  decimal, 0x1F, 1Fh / 0FFh (must start with a digit), 1010b, 17o|17q, 10d
//      JMP/CALL/Jcc (JE/JNE/JL/JGE/JLE/JG/JB/JAE/JBE/JA/JS/JNS/JZ/JNZ/...) label (NEAR rel32)
//  Labels:   name:
//  Comments: ; to end of line
//
//  Build : g++ -std=c++17 -O2 -Wall -Wextra -o final_w1 final_w1.cpp
//  Run   : ./final_w1 test.asm
// =====================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

using namespace std;

// ---------------------------------------------------------------------
// DATA STRUCTURES (shared contract with Worklet 2 and Worklet 3)
// ---------------------------------------------------------------------

// Token types. DIRECTIVE can be added here later.
enum class TokenType {
    MNEMONIC,    // MOV, ADD, ...
    REGISTER,    // EAX, EBX, ... (32-bit, 16-bit, 8-bit)
    IMMEDIATE,   // 5, -3, 0x1000
    IDENTIFIER,  // a name used as an operand (label reference, size directive)
    LABEL,       // name followed by ':'
    COMMA,
    MEMORY       // [eax], [ebx + 4], etc.
};

struct Token {
    TokenType type;
    string    text;
    int       line;
    int       column;   // 1-based
};

struct Operand {
    string       kind;             // "REG", "IMM", "SYM" or "MEM"
    string       value;            // text as written / normalised
    int          column;           // where it starts in the source line
    long long    number;           // numeric value (IMM only)
    bool         resolved;         // SYM only (or MEM symbol): set by Pass 2
    unsigned int resolvedAddress;  // SYM only (or MEM symbol): address of the label

    // Memory / 8-bit / 16-bit operand details
    int          size;             // 8, 16, 32 bits (0 if unspecified)
    string       baseReg;          // Base register: e.g. "EAX", "EBX"
    string       indexReg;         // Index register: e.g. "ECX"
    int          scale;            // Scale: 1, 2, 4, 8
    long long    displacement;     // Displacement offset
    string       symbol;           // Symbol name if referencing a label in MEM
};

struct Instruction {
    string          mnemonic;
    vector<Operand> operands;
    unsigned int    address;       // set by Pass 1
    int             length;        // set by Pass 1 (bytes)
    int             line;
    int             column;
    int             displacement;  // branches only: target - (address+length)
};
