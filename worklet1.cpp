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

// A label found by the parser. instrIndex = index of the instruction that
// follows it (so Pass 1 knows which address the label gets).
struct LabelDef {
    string name;
    int    instrIndex;
    int    line;
    int    column;
};

struct Symbol {
    string       name;
    unsigned int address;
    int          line;      // where it was defined (for duplicate errors)
};

struct AsmError {
    string category;   // "Lexical Error", "Syntax Error", "Symbol Error"
    string message;
    int    line;
    int    column;
};

// Everything Worklet 1 hands over to the next worklets.
struct Worklet1Output {
    vector<Instruction>           instructions;
    unordered_map<string, Symbol> symbolTable;
    vector<AsmError>              errors;
    unsigned int                  finalLC;   // total code size in bytes
    bool                          ok;        // true if no errors at all
};

// ---------------------------------------------------------------------
// SMALL HELPERS
// ---------------------------------------------------------------------

void addError(vector<AsmError>& errors, const string& category,
              const string& message, int line, int column) {
    AsmError e;
    e.category = category;
    e.message  = message;
    e.line     = line;
    e.column   = column;
    errors.push_back(e);
}

string toUpper(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

bool isReg32(const string& r) {
    return r == "EAX" || r == "EBX" || r == "ECX" || r == "EDX" ||
           r == "ESI" || r == "EDI" || r == "ESP" || r == "EBP";
}

bool isReg16(const string& r) {
    return r == "AX" || r == "BX" || r == "CX" || r == "DX" ||
           r == "SI" || r == "DI" || r == "SP" || r == "BP";
}

bool isReg8(const string& r) {
    return r == "AL" || r == "BL" || r == "CL" || r == "DL" ||
           r == "AH" || r == "BH" || r == "CH" || r == "DH";
}

bool isSegmentReg(const string& r) {
    return r == "CS" || r == "DS" || r == "SS" || r == "ES" ||
           r == "FS" || r == "GS";
}

bool isValidRegister(const string& upper) {
    return isReg32(upper) || isReg16(upper) || isReg8(upper) || isSegmentReg(upper);
}

int getRegisterSize(const string& upper) {
    if (isReg32(upper)) return 32;
    if (isReg16(upper)) return 16;
    if (isReg8(upper)) return 8;
    if (isSegmentReg(upper)) return 16;
    return 0;
}

bool isLoadStoreMnemonic(const string& m) {
    return m == "LOAD" || m == "STORE" || m == "LEA";
}

bool isZeroOperandMnemonic(const string& m) {
    return m == "NOP"     || m == "RET"   || m == "CDQ"   || m == "CWD"   ||
           m == "CBW"     || m == "CWDE"  || m == "PUSHAD"|| m == "PUSHA" ||
           m == "POPAD"   || m == "POPA"  || m == "PUSHFD"|| m == "PUSHF" ||
           m == "POPFD"   || m == "POPF"  || m == "CLC"   || m == "STC"   ||
           m == "CMC"     || m == "CLD"   || m == "STD"   || m == "LEAVE" ||
           m == "HLT"     || m == "LODS"  || m == "LODSB" || m == "LODSW" ||
           m == "LODSD"   || m == "STOS"  || m == "STOSB" || m == "STOSW" ||
           m == "STOSD"   ||
           // ---- added: remaining string ops and flag / control instructions ----
           m == "MOVSB"   || m == "MOVSW" || m == "MOVSD" ||
           m == "CMPSB"   || m == "CMPSW" || m == "CMPSD" ||
           m == "SCASB"   || m == "SCASW" || m == "SCASD" ||
             m == "LAHF"    || m == "SAHF"  || m == "CLI"   || m == "STI" ||
           m == "WAIT";
}

bool isCondBranchMnemonic(const string& m) {
    return m == "JE"  || m == "JZ"  || m == "JNE" || m == "JNZ" ||
           m == "JL"  || m == "JNGE"|| m == "JGE" || m == "JNL" ||
           m == "JLE" || m == "JNG" || m == "JG"  || m == "JNLE"||
           m == "JB"  || m == "JNAE"|| m == "JC"  || m == "JAE" ||
           m == "JNB" || m == "JNC" || m == "JBE" || m == "JNA" ||
           m == "JA"  || m == "JNBE"|| m == "JS"  || m == "JNS" ||
           m == "JO"  || m == "JNO" || m == "JP"  || m == "JPE" ||
           m == "JNP" || m == "JPO";
}

// ---- added: LOOP family and JECXZ. They exist only as SHORT jumps (opcode + rel8 = 2 bytes),
// so unlike every other branch their reach is -128..+127 bytes.
bool isLoopMnemonic(const string& m) {
    return m == "LOOP" || m == "LOOPE" || m == "LOOPZ" ||
           m == "LOOPNE" || m == "LOOPNZ" || m == "JECXZ";
}

bool isBranchMnemonic(const string& m) {
    return m == "JMP" || m == "CALL" || isCondBranchMnemonic(m);
}

bool isUnaryRegMnemonic(const string& m) {
    return m == "INC" || m == "DEC" || m == "PUSH" || m == "POP" ||
           m == "NOT" || m == "NEG" || m == "MUL"  || m == "DIV" ||
           m == "IDIV";
}

bool isBinaryArithLogicMnemonic(const string& m) {
    return m == "MOV" || m == "ADD" || m == "SUB" || m == "CMP" || m == "ADC" || m == "SBB" ||
           m == "AND" || m == "OR"  || m == "XOR" || m == "TEST";
}

bool isShiftMnemonic(const string& m) {
    return m == "SHL" || m == "SHR" || m == "SAR" || m == "SAL" ||
           m == "ROL" || m == "ROR" || m == "RCL" || m == "RCR";
}

// How many operands each mnemonic takes. -1 means "not a valid mnemonic".
// To support a new instruction, add it here first.
int expectedOperandCount(const string& m) {
    if (isZeroOperandMnemonic(m)) return 0;
    if (isBranchMnemonic(m)) return 1;
    if (isLoopMnemonic(m)) return 1;
    if (isUnaryRegMnemonic(m)) return 1;
    if (m == "INT") return 1;
    if (isBinaryArithLogicMnemonic(m)) return 2;
    if (isShiftMnemonic(m)) return 2;
    if (m == "XCHG") return 2;
    if (m == "IMUL") return 2;
    if (isLoadStoreMnemonic(m)) return 2;
    return -1;
}

bool isValidMnemonic(const string& upper) {
    return expectedOperandCount(upper) != -1;
}

// Reads numbers that fit in 32 bits:
//     decimal      5   -3   10d
//     hex          0x1F     0FH   1234h      (a suffix form must START WITH A DIGIT: 0FFH, not FFH,
//                                              so that names such as BEEFH stay identifiers)
//     binary       1010b
//     octal        17o  17q
// Returns false for anything else. Never throws.
bool parseNumber(const string& text, long long& value) {
    size_t i = 0;
    bool negative = false;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        if (text[i] == '-') negative = true;
        i++;
    }
    if (i >= text.size()) return false;

    int base = 10;
    size_t end = text.size();
    if (i + 1 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        base = 16;
        i += 2;
        if (i >= text.size()) return false;
    } else if (end - i >= 2 && isdigit((unsigned char)text[i])) {
        // Intel-style radix suffix on the last character
        char sfx = (char)tolower((unsigned char)text[end - 1]);
        if      (sfx == 'h')               { base = 16; end--; }
        else if (sfx == 'b')               { base = 2;  end--; }
        else if (sfx == 'o' || sfx == 'q') { base = 8;  end--; }
        else if (sfx == 'd')               { base = 10; end--; }
    }

    long long result = 0;
    for (; i < end; i++) {
        char c = text[i];
        int digit;
        if (c >= '0' && c <= '9')                 digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
      
