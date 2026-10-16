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

// ---------------------------------------------------------------------
// Function: addError
// Purpose : Constructs a diagnostic AsmError object and appends it to
//           the errors list with source position coordinates.
// @param errors   - Mutable error diagnostic list to append to
// @param category - Classification of error (e.g. "Lexical Error", "Syntax Error")
// @param message  - Human-readable explanation of the issue
// @param line     - 1-based source line number
// @param column   - 1-based source column number
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

// ---------------------------------------------------------------------
// Function: toUpper
// Purpose : Normalizes an ASCII string to uppercase for case-insensitive
//           comparisons of mnemonics and register names.
// @param s - Input string
// @return Uppercased copy of input string
// ---------------------------------------------------------------------
string toUpper(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

// ---------------------------------------------------------------------
// Function: isReg32
// Purpose : Checks whether an uppercase string is one of the 8 general-purpose
//           32-bit x86 registers: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI.
// @param r - Uppercase register name
// @return true if 32-bit register, false otherwise
// ---------------------------------------------------------------------
bool isReg32(const string& r) {
    return r == "EAX" || r == "EBX" || r == "ECX" || r == "EDX" ||
           r == "ESI" || r == "EDI" || r == "ESP" || r == "EBP";
}

// ---------------------------------------------------------------------
// Function: isReg16
// Purpose : Checks whether an uppercase string is one of the 8 general-purpose
//           16-bit x86 registers: AX, CX, DX, BX, SP, BP, SI, DI.
// @param r - Uppercase register name
// @return true if 16-bit register, false otherwise
// ---------------------------------------------------------------------
bool isReg16(const string& r) {
    return r == "AX" || r == "BX" || r == "CX" || r == "DX" ||
           r == "SI" || r == "DI" || r == "SP" || r == "BP";
}

// ---------------------------------------------------------------------
// Function: isReg8
// Purpose : Checks whether an uppercase string is one of the 8 byte-accessible
//           8-bit x86 registers: AL, CL, DL, BL, AH, CH, DH, BH.
// @param r - Uppercase register name
// @return true if 8-bit register, false otherwise
// ---------------------------------------------------------------------
bool isReg8(const string& r) {
    return r == "AL" || r == "BL" || r == "CL" || r == "DL" ||
           r == "AH" || r == "BH" || r == "CH" || r == "DH";
}

// ---------------------------------------------------------------------
// Function: isSegmentReg
// Purpose : Checks whether an uppercase string is one of the 6 x86 segment
//           registers: CS, DS, SS, ES, FS, GS.
// @param r - Uppercase register name
// @return true if segment register, false otherwise
// ---------------------------------------------------------------------
bool isSegmentReg(const string& r) {
    return r == "CS" || r == "DS" || r == "SS" || r == "ES" ||
           r == "FS" || r == "GS";
}

// ---------------------------------------------------------------------
// Function: isValidRegister
// Purpose : Determines whether an uppercase token is a valid x86 architectural
//           register (32-bit, 16-bit, 8-bit, or segment).
// @param upper - Uppercase token string
// @return true if valid register, false otherwise
// ---------------------------------------------------------------------
bool isValidRegister(const string& upper) {
    return isReg32(upper) || isReg16(upper) || isReg8(upper) || isSegmentReg(upper);
}

// ---------------------------------------------------------------------
// Function: getRegisterSize
// Purpose : Returns the bit-width of a valid register (8, 16, or 32 bits).
//           Returns 0 if the register is unrecognized or segment-only.
// @param upper - Uppercase register name
// @return Bit size (8, 16, 32, or 0)
// ---------------------------------------------------------------------
int getRegisterSize(const string& upper) {
    if (isReg32(upper)) return 32;
    if (isReg16(upper)) return 16;
    if (isReg8(upper)) return 8;
    if (isSegmentReg(upper)) return 16;
    return 0;
}

// ---------------------------------------------------------------------
// Function: isLoadStoreMnemonic
// Purpose : Checks whether a mnemonic represents memory transfer or address
//           calculation: LOAD, STORE, or LEA.
// @param m - Uppercase mnemonic string
// @return true if LOAD/STORE/LEA, false otherwise
// ---------------------------------------------------------------------
bool isLoadStoreMnemonic(const string& m) {
    return m == "LOAD" || m == "STORE" || m == "LEA";
}

// ---------------------------------------------------------------------
// Function: isZeroOperandMnemonic
// Purpose : Checks whether a mnemonic takes 0 operands (flags, string instructions,
//           stack frame setup/teardown, halt, interrupts, etc.).
// @param m - Uppercase mnemonic string
// @return true if zero-operand instruction, false otherwise
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
// Function: isCondBranchMnemonic
// Purpose : Identifies conditional jump mnemonics (JE, JNE, JZ, JNZ, JA, JB,
//           JL, JG, JLE, JGE, JS, JNS, etc.) that take a single label target.
// @param m - Uppercase mnemonic string
// @return true if conditional branch, false otherwise
// ---------------------------------------------------------------------
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
// ---------------------------------------------------------------------
// Function: isLoopMnemonic
// Purpose : Identifies loop control and count-branch instructions:
//           LOOP, LOOPE, LOOPZ, LOOPNE, LOOPNZ, JECXZ (short rel8 branches).
// @param m - Uppercase mnemonic string
// @return true if loop/jecxz instruction, false otherwise
// ---------------------------------------------------------------------
bool isLoopMnemonic(const string& m) {
    return m == "LOOP" || m == "LOOPE" || m == "LOOPZ" ||
           m == "LOOPNE" || m == "LOOPNZ" || m == "JECXZ";
}

// ---------------------------------------------------------------------
// Function: isBranchMnemonic
// Purpose : Identifies any control-flow branch: JMP, CALL, conditional jumps,
//           or loop instructions.
// @param m - Uppercase mnemonic string
// @return true if any branch or call instruction, false otherwise
// ---------------------------------------------------------------------
bool isBranchMnemonic(const string& m) {
    return m == "JMP" || m == "CALL" || isCondBranchMnemonic(m);
}

// ---------------------------------------------------------------------
// Function: isUnaryRegMnemonic
// Purpose : Identifies single-operand register/memory operations:
//           INC, DEC, PUSH, POP, NOT, NEG, MUL, DIV, IDIV.
// @param m - Uppercase mnemonic string
// @return true if unary instruction, false otherwise
// ---------------------------------------------------------------------
bool isUnaryRegMnemonic(const string& m) {
    return m == "INC" || m == "DEC" || m == "PUSH" || m == "POP" ||
           m == "NOT" || m == "NEG" || m == "MUL"  || m == "DIV" ||
           m == "IDIV";
}

// ---------------------------------------------------------------------
// Function: isBinaryArithLogicMnemonic
// Purpose : Identifies two-operand arithmetic and logical instructions:
//           MOV, ADD, ADC, SUB, SBB, CMP, AND, OR, XOR, TEST.
// @param m - Uppercase mnemonic string
// @return true if binary arithmetic/logical instruction, false otherwise
// ---------------------------------------------------------------------
bool isBinaryArithLogicMnemonic(const string& m) {
    return m == "MOV" || m == "ADD" || m == "SUB" || m == "CMP" || m == "ADC" || m == "SBB" ||
           m == "AND" || m == "OR"  || m == "XOR" || m == "TEST";
}

// ---------------------------------------------------------------------
// Function: isShiftMnemonic
// Purpose : Identifies bitwise shift and rotate instructions:
//           SHL, SHR, SAR, SAL, ROL, ROR, RCL, RCR.
// @param m - Uppercase mnemonic string
// @return true if shift/rotate instruction, false otherwise
// ---------------------------------------------------------------------
bool isShiftMnemonic(const string& m) {
    return m == "SHL" || m == "SHR" || m == "SAR" || m == "SAL" ||
           m == "ROL" || m == "ROR" || m == "RCL" || m == "RCR";
}

// How many operands each mnemonic takes. -1 means "not a valid mnemonic".
// To support a new instruction, add it here first.
// ---------------------------------------------------------------------
// Function: expectedOperandCount
// Purpose : Returns the required number of operands (0, 1, or 2) for an
//           instruction mnemonic. Returns -1 if the mnemonic is unsupported.
// @param m - Uppercase mnemonic string
// @return Expected operand count (0, 1, 2) or -1 if invalid
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
// Function: isValidMnemonic
// Purpose : Checks if a token is a recognized assembler mnemonic by querying
//           expectedOperandCount().
// @param upper - Uppercase token string
// @return true if valid mnemonic, false otherwise
// ---------------------------------------------------------------------
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
// ---------------------------------------------------------------------
// Function: parseNumber
// Purpose : Parses numeric literals in 5 supported radix formats:
//           - Hexadecimal : 0x1F or 1Fh / 0FFh (must begin with a digit)
//           - Binary      : 1010b
//           - Octal       : 17o or 17q
//           - Decimal     : 10d or plain 42, with optional leading +/- sign.
//           Validates 32-bit signed and unsigned range limits.
// @param text  - Literal string to parse
// @param value - Output parameter receiving parsed 64-bit value
// @return true if parsing succeeds within 32-bit range, false otherwise
// ---------------------------------------------------------------------
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
        else return false;
        if (digit >= base) return false;           // e.g. 102b, 19o

        result = result * base + digit;
        if (result > 4294967295LL) return false;   // more than 32 bits
    }
    if (negative) {
        result = -result;
        if (result < -2147483648LL) return false;
    }
    value = result;
    return true;
}

// Parses inside of [ ... ], e.g. "EAX", "EBX + 4", "EBP - 8", "0x1000", "start", "EAX + ECX*4 + 8"
// ---------------------------------------------------------------------
// Function: parseMemoryExpression
// Purpose : Decomposes a memory operand inside brackets [...] into its SIB
//           components: base register, index register, scale multiplier,
//           numeric displacement, and optional symbolic label name.
//           Validates that scale is in {1, 2, 4, 8} and index != ESP.
// @param inside       - Content string inside the brackets [...]
// @param baseReg      - Output parameter: detected base register (e.g. "EBX")
// @param indexReg     - Output parameter: detected index register (e.g. "ECX")
// @param scale        - Output parameter: scaling factor (1, 2, 4, or 8)
// @param displacement - Output parameter: combined numeric constant offset
// @param symbol       - Output parameter: symbolic label name if referenced
// @return true if expression syntax is valid, false if syntax error
// ---------------------------------------------------------------------
bool parseMemoryExpression(const string& inside,
                           string& baseReg, string& indexReg, int& scale,
                           long long& displacement, string& symbol) {
    string str;
    for (size_t k = 0; k < inside.size(); k++) {
        if (!isspace((unsigned char)inside[k])) str += inside[k];
    }
    if (str.empty()) return false;

    vector<string> terms;
    vector<char> ops;
    size_t i = 0;
    if (str[0] == '+' || str[0] == '-') {
        ops.push_back(str[0]);
        i = 1;
    } else {
        ops.push_back('+');
    }

    size_t termStart = i;
    for (; i <= str.size(); i++) {
        if (i == str.size() || str[i] == '+' || str[i] == '-') {
            if (i > termStart) {
                terms.push_back(str.substr(termStart, i - termStart));
            }
            if (i < str.size()) {
                ops.push_back(str[i]);
                termStart = i + 1;
            }
        }
    }

    if (terms.size() != ops.size()) return false;

    baseReg = "";
    indexReg = "";
    scale = 1;
    displacement = 0;
    symbol = "";

    for (size_t t = 0; t < terms.size(); t++) {
        string term = terms[t];
        char sign = ops[t];
        long long mult = (sign == '-') ? -1 : 1;

        // Check for scaled index: e.g. ECX*4 or 4*ECX
        size_t starPos = term.find('*');
        if (starPos != string::npos) {
            string p1 = term.substr(0, starPos);
            string p2 = term.substr(starPos + 1);
            string rPart, sPart;
            if (isValidRegister(toUpper(p1))) {
                rPart = toUpper(p1);
                sPart = p2;
            } else if (isValidRegister(toUpper(p2))) {
                rPart = toUpper(p2);
                sPart = p1;
            } else {
                return false;
            }
            long long scVal = 1;
            if (!parseNumber(sPart, scVal) || (scVal != 1 && scVal != 2 && scVal != 4 && scVal != 8)) {
                return false;
            }
            indexReg = rPart;
            scale = (int)scVal;
            continue;
        }

        // Check if term is a register
        string upperTerm = toUpper(term);
        if (isValidRegister(upperTerm)) {
            if (baseReg.empty()) {
                baseReg = upperTerm;
            } else if (indexReg.empty()) {
                indexReg = upperTerm;
                scale = 1;
            } else {
                return false;
            }
            continue;
        }

        // Check if term is a numeric displacement
        long long numVal = 0;
        if (parseNumber(term, numVal)) {
            displacement += mult * numVal;
            continue;
        }

        // Identifier / symbol name (e.g. [start])
        if (isalpha((unsigned char)term[0]) || term[0] == '_') {
            symbol = term;
            continue;
        }

        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// Function: makeOperand
// Purpose : Factory helper to construct an Operand struct with normalized
//           default properties and source line column tracking.
// @param kind   - Operand kind: "REG", "IMM", "MEM", or "SYM"
// @param value  - Raw/normalized text representation
// @param column - 1-based source column position
// @param number - Numeric value (for IMM operands)
// @param size   - Explicit size in bits (8, 16, 32, or 0)
// @return Fully populated Operand struct
// ---------------------------------------------------------------------
Operand makeOperand(const string& kind, const string& value, int column) {
    Operand op;
    op.kind            = kind;
    op.value           = value;
    op.column          = column;
    op.number          = 0;
    op.resolved        = false;
    op.resolvedAddress = 0;
    op.size            = 0;
    op.baseReg         = "";
    op.indexReg        = "";
    op.scale           = 1;
    op.displacement    = 0;
    op.symbol          = "";
    return op;
}

// =====================================================================
//  POOJA : Lexer / Tokenizer
//  Turns ONE source line into tokens. Every token remembers its line and
//  column. Bad characters / bad numbers are reported as errors; the lexer
//  never crashes and never reads outside the string.
// =====================================================================
// ---------------------------------------------------------------------
// Function: tokenizeLine
// Purpose : Lexical analyzer for a single line of assembly text. Skips comments
//           starting with ';', isolates bracketed memory references '[...]',
//           and classifies tokens into MNEMONIC, REGISTER, IMMEDIATE, IDENTIFIER,
//           LABEL, COMMA, or MEMORY.
// @param line       - Raw source line text
// @param lineNumber - 1-based line number for error reporting
// @param errors     - Mutable error collection for logging lexical diagnostics
// @return Vector of classified Token objects
// ---------------------------------------------------------------------
vector<Token> tokenizeLine(const string& line, int lineNumber, vector<AsmError>& errors) {
    vector<Token> tokens;
    size_t i = 0;

    while (i < line.size()) {
        char c = line[i];
        int column = (int)i + 1;

        // skip white space
        if (isspace((unsigned char)c)) { i++; continue; }

        // comment: ignore the rest of the line
        if (c == ';') break;

        // comma
        if (c == ',') {
            tokens.push_back({TokenType::COMMA, ",", lineNumber, column});
            i++;
            continue;
        }

        // memory bracket: [ ... ]
        if (c == '[') {
            size_t start = i;
            int bracketColumn = column;
            i++; // skip '['
            while (i < line.size() && line[i] != ']' && line[i] != ';') i++;
            if (i < line.size() && line[i] == ']') {
                i++; // include ']'
                string memText = line.substr(start, i - start);
                tokens.push_back({TokenType::MEMORY, memText, lineNumber, bracketColumn});
            } else {
                addError(errors, "Lexical Error", "missing closing ']' for memory operand",
                         lineNumber, bracketColumn);
            }
            continue;
        }

        // number: starts with a digit, or '-' / '+' followed by a digit
        if (isdigit((unsigned char)c) ||
            ((c == '-' || c == '+') && i + 1 < line.size() && isdigit((unsigned char)line[i + 1]))) {
            size_t start = i;
            i++;
            while (i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            string text = line.substr(start, i - start);

            long long ignored;
            if (parseNumber(text, ignored)) {
                tokens.push_back({TokenType::IMMEDIATE, text, lineNumber, column});
            } else {
                addError(errors, "Lexical Error", "invalid immediate '" + text + "'",
                         lineNumber, column);
            }
            continue;
        }

        // word: mnemonic, register, label or identifier
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            string word  = line.substr(start, i - start);
            string upper = toUpper(word);

            bool followedByColon = (i < line.size() && line[i] == ':');

            if (followedByColon) {
                i++;   // consume ':'
                if (isValidMnemonic(upper) || isValidRegister(upper)) {
                    addError(errors, "Lexical Error",
                             "label name '" + word + "' is a reserved word", lineNumber, column);
                } else {
                    tokens.push_back({TokenType::LABEL, word, lineNumber, column});
                }
            } else if (isValidMnemonic(upper)) {
                tokens.push_back({TokenType::MNEMONIC, upper, lineNumber, column});
            } else if (isValidRegister(upper)) {
                tokens.push_back({TokenType::REGISTER, upper, lineNumber, column});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, word, lineNumber, column});
            }
            continue;
        }

        // anything else is not part of our language
        addError(errors, "Lexical Error", string("invalid character '") + c + "'",
                 lineNumber, column);
        i++;
    }
    return tokens;
}

// =====================================================================
//  OM : Parser  (tokens -> Instruction IR)
//  Grammar of one line:   [LABEL] [MNEMONIC [operand {, operand}]]
// =====================================================================

// Checks that the operand KINDS are legal for this mnemonic.
// ---------------------------------------------------------------------
// Function: checkOperandKinds
// Purpose : Semantic validator enforcing x86 architecture constraints:
//           - Prevents memory-to-memory moves (e.g. MOV [mem], [mem])
//           - Enforces operand size matching between registers and memory
//           - Rejects immediate destination operands
//           - Validates jump/branch targets are symbols/labels
// @param instr  - Intermediate instruction representation to validate
// @param errors - Mutable error collection for logging semantic errors
// @return true if instruction obeys all x86 semantic rules, false otherwise
// ---------------------------------------------------------------------
bool checkOperandKinds(const Instruction& instr, vector<AsmError>& errors) {
    const string& m = instr.mnemonic;
    bool ok = true;

    if (m == "MOV") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];

        if (a.kind != "REG" && a.kind != "MEM") {
            addError(errors, "Syntax Error", "destination must be a register or memory operand",
                     instr.line, a.column);
            ok = false;
        }
        if (a.kind == "MEM" && b.kind == "MEM") {
            addError(errors, "Syntax Error", "memory to memory move is not supported",
                     instr.line, b.column);
            ok = false;
        }
        if (b.kind != "REG" && b.kind != "IMM" && b.kind != "MEM") {
            addError(errors, "Syntax Error", "invalid source operand '" + b.value + "'",
                     instr.line, b.column);
            ok = false;
        }

        // Size mismatch checks
        if (a.kind == "REG" && b.kind == "REG") {
            if (getRegisterSize(a.value) != getRegisterSize(b.value)) {
                addError(errors, "Syntax Error",
                         "operand size mismatch between '" + a.value + "' and '" + b.value + "'",
                         instr.line, b.column);
                ok = false;
            }
        } else if (a.kind == "REG" && b.kind == "MEM") {
            if (b.size != 0 && getRegisterSize(a.value) != b.size) {
                addError(errors, "Syntax Error",
                         "operand size mismatch between register '" + a.value + "' and memory operand",
                         instr.line, b.column);
                ok = false;
            }
        } else if (a.kind == "MEM" && b.kind == "REG") {
            if (a.size != 0 && a.size != getRegisterSize(b.value)) {
                addError(errors, "Syntax Error",
                         "operand size mismatch between memory operand and register '" + b.value + "'",
                         instr.line, b.column);
                ok = false;
            }
        } else if (a.kind == "REG" && b.kind == "IMM") {
            int rSize = getRegisterSize(a.value);
            if (rSize == 8 && (b.number < -128 || b.number > 255)) {
                addError(errors, "Syntax Error",
                         "immediate '" + b.value + "' exceeds 8-bit range",
                         instr.line, b.column);
                ok = false;
            } else if (rSize == 16 && (b.number < -32768 || b.number > 65535)) {
                addError(errors, "Syntax Error",
                         "immediate '" + b.value + "' exceeds 16-bit range",
                         instr.line, b.column);
                ok = false;
            }
        }
    }
    else if (m == "LOAD") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "REG") {
            addError(errors, "Syntax Error", "LOAD destination must be a register",
                     instr.line, a.column);
            ok = false;
        }
        if (b.kind != "MEM" && b.kind != "REG" && b.kind != "IMM" && b.kind != "SYM") {
            addError(errors, "Syntax Error", "LOAD source must be a memory, register, immediate, or address operand",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (m == "STORE") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "MEM" && a.kind != "REG" && a.kind != "IMM" && a.kind != "SYM") {
            addError(errors, "Syntax Error", "STORE destination must be a memory, register, or address operand",
                     instr.line, a.column);
            ok = false;
        }
        if (b.kind != "REG") {
            addError(errors, "Syntax Error", "STORE source must be a register",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (m == "LEA") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "REG") {
            addError(errors, "Syntax Error", "LEA destination must be a register",
                     instr.line, a.column);
            ok = false;
        }
        // A bare label is accepted too: "LEA EDX, label" == "LEA EDX, [label]" (address of the label).
        if (b.kind != "MEM" && b.kind != "SYM") {
            addError(errors, "Syntax Error", "LEA source must be a memory operand",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (m == "ADD" || m == "SUB" || m == "CMP" || m == "ADC" || m == "SBB" ||
             m == "AND" || m == "OR"  || m == "XOR" || m == "TEST") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "REG" && a.kind != "MEM") {
            string msg = (a.kind == "SYM") ? "invalid register '" + a.value + "'"
                                           : "first operand must be a register or memory";
            addError(errors, "Syntax Error", msg, instr.line, a.column);
            ok = false;
        }
        if (a.kind == "MEM" && b.kind == "MEM") {
            addError(errors, "Syntax Error", "two memory operands not allowed",
                     instr.line, b.column);
            ok = false;
        }
        if (b.kind != "REG" && b.kind != "IMM" && b.kind != "MEM") {
            addError(errors, "Syntax Error",
                     "invalid second operand '" + b.value + "'",
                     instr.line, b.column);
            ok = false;
        }
        if (a.kind == "REG" && b.kind == "REG" && getRegisterSize(a.value) != getRegisterSize(b.value)) {
            addError(errors, "Syntax Error",
                     "operand size mismatch between '" + a.value + "' and '" + b.value + "'",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (m == "XCHG") {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "REG" && a.kind != "MEM") {
            string msg = (a.kind == "SYM") ? "invalid register '" + a.value + "'"
                                           : "first operand must be a register or memory";
            addError(errors, "Syntax Error", msg, instr.line, a.column);
            ok = false;
        }
        if (b.kind != "REG" && (b.kind != "MEM" || a.kind != "REG")) {
            string msg = (b.kind == "SYM") ? "invalid register '" + b.value + "'"
                                           : "second operand must be a register";
            addError(errors, "Syntax Error", msg, instr.line, b.column);
            ok = false;
        }
        if (a.kind == "REG" && b.kind == "REG" && getRegisterSize(a.value) != getRegisterSize(b.value)) {
            addError(errors, "Syntax Error",
                     "operand size mismatch between '" + a.value + "' and '" + b.value + "'",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (isShiftMnemonic(m)) {
        const Operand& a = instr.operands[0];
        const Operand& b = instr.operands[1];
        if (a.kind != "REG" && a.kind != "MEM") {
            string msg = (a.kind == "SYM") ? "invalid register '" + a.value + "'"
                                           : "first operand must be a register or memory";
            addError(errors, "Syntax Error", msg, instr.line, a.column);
            ok = false;
        }
        if (b.kind != "IMM" && b.kind != "REG") {
            addError(errors, "Syntax Error",
                     "invalid shift count '" + b.value + "' (expected immediate or register)",
                     instr.line, b.column);
            ok = false;
        }
    }
    else if (m == "IMUL") {
        const Operand& a = instr.operands[0];
        if (a.kind != "REG") {
            string msg = (a.kind == "SYM") ? "invalid register '" + a.value + "'"
                                           : "first operand must be a register";
            addError(errors, "Syntax Error", msg, instr.line, a.column);
            ok = false;
        }
        if (instr.operands.size() > 1) {
            const Operand& b = instr.operands[1];
            if (b.kind != "REG" && b.kind != "IMM" && b.kind != "MEM") {
                addError(errors, "Syntax Error",
                         "invalid operand '" + b.value + "' (expected register, memory or immediate)",
                         instr.line, b.column);
                ok = false;
            }
        }
    }
    else if (isUnaryRegMnemonic(m)) {
        const Operand& a = instr.operands[0];
        if (a.kind != "REG" && a.kind != "MEM") {
            string msg = (a.kind == "SYM") ? "invalid register '" + a.value + "'"
                                           : "operand must be a register or memory";
            addError(errors, "Syntax Error", msg, instr.line, a.column);
            ok = false;
        }
        if ((m == "PUSH" || m == "POP") && a.kind == "REG" && isReg8(a.value)) {
            addError(errors, "Syntax Error", "cannot " + m + " an 8-bit register",
                     instr.line, a.column);
            ok = false;
        }
    }
    else if (m == "INT") {
        const Operand& a = instr.operands[0];
        if (a.kind != "IMM") {
            addError(errors, "Syntax Error",
                     "operand must be an immediate interrupt number",
                     instr.line, a.column);
            ok = false;
        }
    }
    else if (isLoopMnemonic(m)) {
        const Operand& a = instr.operands[0];
        if (a.kind != "SYM") {
            addError(errors, "Syntax Error", m + " needs a label as its target",
                     instr.line, a.column);
            ok = false;
        }
    }
    else if (isBranchMnemonic(m)) {
        const Operand& a = instr.operands[0];
        if (a.kind != "SYM" && a.kind != "REG" && a.kind != "MEM") {
            addError(errors, "Syntax Error", "expected a label, register, or memory as branch target",
                     instr.line, a.column);
            ok = false;
        }
    }
    return ok;
}

// Parses tokens[start..end] as one instruction. Returns false on error
// (the error is already recorded).
// ---------------------------------------------------------------------
// Function: parseInstruction
// Purpose : Parses token sequence into an Instruction IR node. Handles operand
//           comma separation, size specifiers (BYTE/WORD/DWORD PTR), delegates
//           bracket evaluation to parseMemoryExpression, and runs semantic checks.
// @param tokens      - Vector of tokens from the lexer
// @param startIdx    - Token index where the instruction mnemonic begins
// @param lineNumber  - Source line number for diagnostic logging
// @param outInstr    - Output parameter receiving successfully parsed Instruction
// @param errors      - Mutable error collection for syntax/semantic errors
// @return true if successfully parsed and semantically valid, false otherwise
// ---------------------------------------------------------------------
bool parseInstruction(const vector<Token>& tokens, size_t start,
                      Instruction& instr, vector<AsmError>& errors) {
    const Token& first = tokens[start];
    instr.line         = first.line;
    instr.column       = first.column;
    instr.address      = 0;
    instr.length       = 0;
    instr.displacement = 0;

    // 1. first token must be a valid mnemonic
    if (first.type != TokenType::MNEMONIC) {
        if (first.type == TokenType::IDENTIFIER)
            addError(errors, "Syntax Error", "invalid mnemonic '" + first.text + "'",
                     first.line, first.column);
        else
            addError(errors, "Syntax Error", "expected a mnemonic, found '" + first.text + "'",
                     first.line, first.column);
        return false;
    }
    instr.mnemonic = first.text;

    // 2. read operands:  operand {, operand}
    size_t idx = start + 1;
    while (idx < tokens.size()) {
        const Token& t = tokens[idx];
        Operand op;

        int explicitSize = 0;
        string prefixText = "";
        int startCol = t.column;

        // Check for size directives: BYTE, WORD, DWORD, QWORD, PTR
        if (t.type == TokenType::IDENTIFIER) {
            string u = toUpper(t.text);
            if (u == "BYTE" || u == "WORD" || u == "DWORD" || u == "QWORD") {
                if (u == "BYTE") explicitSize = 8;
                else if (u == "WORD") explicitSize = 16;
                else if (u == "DWORD") explicitSize = 32;
                else if (u == "QWORD") explicitSize = 64;
                prefixText = u;
                idx++;
                if (idx < tokens.size() && tokens[idx].type == TokenType::IDENTIFIER &&
                    toUpper(tokens[idx].text) == "PTR") {
                    prefixText += " PTR";
                    idx++;
                }
            } else if (u == "PTR") {
                prefixText = "PTR";
                idx++;
            }
        }

        if (idx >= tokens.size()) {
            addError(errors, "Syntax Error", "missing operand after size directive",
                     t.line, t.column + (int)t.text.size());
            return false;
        }

        const Token& curr = tokens[idx];

        if (curr.type == TokenType::MEMORY) {
            string memRaw = curr.text;
            string inside = (memRaw.size() >= 2) ? memRaw.substr(1, memRaw.size() - 2) : "";
            string val = (!prefixText.empty()) ? (prefixText + " " + memRaw) : memRaw;
            op = makeOperand("MEM", val, startCol);
            op.size = explicitSize;
            if (!parseMemoryExpression(inside, op.baseReg, op.indexReg, op.scale,
                                       op.displacement, op.symbol)) {
                addError(errors, "Syntax Error", "invalid memory expression '" + memRaw + "'",
                         curr.line, curr.column);
                return false;
            }
        } else if (curr.type == TokenType::REGISTER) {
            op = makeOperand("REG", curr.text, startCol);
            op.size = getRegisterSize(curr.text);
        } else if (curr.type == TokenType::IMMEDIATE) {
            op = makeOperand("IMM", curr.text, startCol);
            op.size = explicitSize;
            parseNumber(curr.text, op.number);   // already validated by the lexer
        } else if (curr.type == TokenType::IDENTIFIER) {
            op = makeOperand("SYM", curr.text, startCol);
            op.size = explicitSize;
        } else {
            addError(errors, "Syntax Error", "expected an operand, found '" + curr.text + "'",
                     curr.line, curr.column);
            return false;
        }
        instr.operands.push_back(op);
        idx++;

        if (idx >= tokens.size()) break;               // end of line: done

        if (tokens[idx].type != TokenType::COMMA) {    // operands must be separated by ','
            addError(errors, "Syntax Error", "expected comma",
                     tokens[idx].line, tokens[idx].column);
            return false;
        }
        idx++;                                         // skip the comma

        if (idx >= tokens.size()) {                    // line ended right after a comma
            const Token& comma = tokens[idx - 1];
            addError(errors, "Syntax Error", "missing operand after comma",
                     comma.line, comma.column + 1);
            return false;
        }
    }

    // Infer memory operand size if unspecified and the other operand is a register
    if (instr.operands.size() == 2) {
        if (instr.operands[0].kind == "REG" && instr.operands[1].kind == "MEM" && instr.operands[1].size == 0) {
            instr.operands[1].size = instr.operands[0].size;
        } else if (instr.operands[0].kind == "MEM" && instr.operands[1].kind == "REG" && instr.operands[0].size == 0) {
            instr.operands[0].size = instr.operands[1].size;
        }
    }

    // 3. operand COUNT must match the mnemonic
    int expected = expectedOperandCount(instr.mnemonic);
    int actual   = (int)instr.operands.size();

    bool countOk = (actual == expected);
    if (instr.mnemonic == "IMUL" && (actual == 1 || actual == 2)) countOk = true;
    if ((instr.mnemonic == "LODS" || instr.mnemonic == "STOS") && (actual == 0 || actual == 1)) countOk = true;

    if (!countOk) {
        if (actual < expected) {
            const Token& last = tokens.back();
            addError(errors, "Syntax Error",
                     "missing operand: " + instr.mnemonic + " expects " + to_string(expected) +
                     " operand(s), found " + to_string(actual),
                     last.line, last.column + (int)last.text.size());
            return false;
        }
        if (actual > expected) {
            int errCol = (expected < actual) ? instr.operands[expected].column : (tokens.back().column + (int)tokens.back().text.size());
            addError(errors, "Syntax Error",
                     "too many operands: " + instr.mnemonic + " expects " + to_string(expected) +
                     " operand(s), found " + to_string(actual),
                     instr.line, errCol);
            return false;
        }
    }

    // 4. operand KINDS must be legal for the mnemonic
    return checkOperandKinds(instr, errors);
}

// Handles one whole line: optional label, then optional instruction.
// ---------------------------------------------------------------------
// Function: parseLine
// Purpose : Coordinates line-level parsing. Strips optional leading label
//           (e.g. "loop_start:"), records it in the labels list associated
//           with the upcoming instruction index, and invokes parseInstruction().
// @param tokens       - Vector of tokens on this line
// @param instructions - Mutable list of instructions to append to
// @param labels       - Mutable list of label definitions to append to
// @param errors       - Mutable error collection for diagnostics
// ---------------------------------------------------------------------
void parseLine(const vector<Token>& tokens, vector<Instruction>& instructions,
               vector<LabelDef>& labels, vector<AsmError>& errors) {
    if (tokens.empty()) return;

    size_t idx = 0;
    if (tokens[0].type == TokenType::LABEL) {
        LabelDef label;
        label.name       = tokens[0].text;
        label.instrIndex = (int)instructions.size();   // the next instruction
        label.line       = tokens[0].line;
        label.column     = tokens[0].column;
        labels.push_back(label);
        idx = 1;
    }
    if (idx >= tokens.size()) return;                  // label-only line

    Instruction instr;
    if (parseInstruction(tokens, idx, instr, errors)) {
        instructions.push_back(instr);
    }
}

// =====================================================================
//  VED : Symbol Table
// =====================================================================
// ---------------------------------------------------------------------
// Function: isDefined
// Purpose : Queries the symbol table to check if a label has already been
//           defined.
// @param table - Symbol table map
// @param name  - Identifier/label name to look up
// @return true if symbol exists, false otherwise
// ---------------------------------------------------------------------
bool isDefined(const unordered_map<string, Symbol>& table, const string& name) {
    return table.find(name) != table.end();
}

// Returns false if the name already exists (duplicate symbol).
// ---------------------------------------------------------------------
// Function: addSymbol
// Purpose : Inserts a newly defined symbol into the symbol table. Prevents
//           duplicate definitions by returning false if the name already exists.
// @param table   - Mutable symbol table map
// @param name    - Unique symbol name
// @param address - Virtual byte address allocated by Pass 1 Location Counter
// @param line    - Source line number of definition
// @return true if inserted successfully, false if duplicate symbol
// ---------------------------------------------------------------------
bool addSymbol(unordered_map<string, Symbol>& table, const string& name,
               unsigned int address, int line) {
    if (isDefined(table, name)) return false;
    Symbol s;
    s.name    = name;
    s.address = address;
    s.line    = line;
    table[name] = s;
    return true;
}

// =====================================================================
//  VED : Pass 1  (Location Counter + label addresses)
//
//  Instruction sizes (chosen so they match a real x86-32 assembler, and
//  fixed so no size guessing is needed later):
//      MOV reg, imm          5      (B8+r imm32)
//      MOV reg, reg          2
//      ADD/SUB/CMP reg, reg  2
//      ADD/SUB/CMP reg, imm  3 if imm fits in signed 8 bits (83 /r ib)
//                            5 if reg is EAX (short form: op imm32)
//                            6 otherwise (81 /r id)
//      INC/DEC/PUSH/POP reg  1
//      NOP / RET             1
//      JMP / CALL            5      (near, rel32)
//      JE / JNE              6      (near, 0F 8x rel32)
//  Branches always use the near form so their size never depends on the
//  distance to the label. Worklet 2 must encode the same near forms.
// =====================================================================
// ---------------------------------------------------------------------
// Function: fitsInt8
// Purpose : Checks if a numeric displacement or immediate fits within a
//           signed 8-bit integer range (-128 to +127). Handles sign-wrapped
//           32-bit values.
// @param n - Numeric value to evaluate
// @return true if fits in signed 8 bits, false otherwise
// ---------------------------------------------------------------------
bool fitsInt8(long long n) {
    if (n > 2147483647LL) n -= 4294967296LL;   // 0xFFFFFFFF is really -1
    return n >= -128 && n <= 127;
}

// ---------------------------------------------------------------------
// Function: getMemoryOperandDisplacementSize
// Purpose : Determines the byte width of the displacement field for an x86
//           memory operand:
//           - 4 bytes for symbolic addresses, bare [disp32], or large offsets
//           - 1 byte if displacement fits in signed 8 bits (disp8)
//           - 1 byte for [EBP] base with 0 displacement (x86 ModR/M rule)
//           - 0 bytes if no displacement needed
// @param op - Memory operand struct
// @return Displacement byte size (0, 1, or 4)
// ---------------------------------------------------------------------
int getMemoryOperandDisplacementSize(const Operand& op) {
    if (!op.symbol.empty()) return 4;
    if (op.baseReg.empty()) return 4;          // [disp32] and [index*scale+disp32]
    if (op.displacement == 0) {
        if (op.baseReg == "EBP" || op.baseReg == "BP") return 1;
        return 0;
    }
    if (fitsInt8(op.displacement)) return 1;
    return 4;
}

// ---------------------------------------------------------------------
// Function: hasSIB
// Purpose : Checks if an addressing mode requires an x86 SIB (Scale-Index-Base)
//           byte. An SIB byte is required if an index register is used or
//           if ESP is the base register.
// @param op - Memory operand struct
// @return true if SIB byte required, false otherwise
// ---------------------------------------------------------------------
bool hasSIB(const Operand& op) {
    if (!op.indexReg.empty()) return true;
    if (op.baseReg == "ESP") return true;
    return false;
}

// Bytes after the opcode that a memory operand adds: SIB + displacement.
// ---------------------------------------------------------------------
// Function: memExtraLength
// Purpose : Computes extra bytes following the ModR/M byte for a memory operand:
//           (1 byte SIB if present) + (0, 1, or 4 bytes displacement).
// @param op - Memory operand struct
// @return Total extra memory address bytes
// ---------------------------------------------------------------------
int memExtraLength(const Operand& op) {
    return (hasSIB(op) ? 1 : 0) + getMemoryOperandDisplacementSize(op);
}

// Data size (8/16/32) of the operation: a register operand decides, else a sized
// memory operand, else 32. Mirrors how Worklet 2 resolves it.
// ---------------------------------------------------------------------
// Function: operationBits
// Purpose : Resolves the operational bit-width (8, 16, or 32 bits) of an
//           instruction based on register size or explicit memory size prefix.
//           Defaults to 32 bits if unspecified.
// @param instr - Instruction IR node
// @return Data size in bits (8, 16, or 32)
// ---------------------------------------------------------------------
int operationBits(const Instruction& instr) {
    int memBits = 0;
    for (size_t k = 0; k < instr.operands.size(); k++) {
        const Operand& o = instr.operands[k];
        if (o.kind == "REG") return getRegisterSize(o.value);
        if (o.kind == "MEM" && o.size != 0) memBits = o.size;
    }
    return memBits ? memBits : 32;
}

// Size of an immediate field for an operation of this many bits.
// ---------------------------------------------------------------------
// Function: immFieldBytes
// Purpose : Computes the byte size of an immediate value field according
//           to the operation bit-width (8 bits -> 1 byte, 16 bits -> 2 bytes,
//           32 bits -> 4 bytes).
// @param bits - Operation bit-width (8, 16, or 32)
// @return Immediate field size in bytes (1, 2, or 4)
// ---------------------------------------------------------------------
int immFieldBytes(int bits) { return bits == 8 ? 1 : (bits == 16 ? 2 : 4); }

// ---------------------------------------------------------------------
// Function: getInstructionLength
// Purpose : Core sizing engine. Calculates the exact x86-32 machine-code byte
//           length of an instruction by evaluating prefix bytes (0x66 for 16-bit),
//           primary and secondary opcodes, ModR/M, SIB, displacement, and immediate
//           payload fields. Uses deterministic fixed near sizes for jumps (5/6 bytes).
// @param instr - Instruction IR node to size
// @return Total instruction length in bytes
// ---------------------------------------------------------------------
int getInstructionLength(const Instruction& instr) {
    const string& m = instr.mnemonic;

    // Check if any operand is 16-bit (prefix 0x66)
    bool is16 = false;
    for (size_t k = 0; k < instr.operands.size(); k++) {
        if (instr.operands[k].size == 16) is16 = true;
        if (instr.operands[k].kind == "REG" && isReg16(instr.operands[k].value)) is16 = true;
    }
    int prefixLen = is16 ? 1 : 0;

    // Memory operations: MOV / LOAD / STORE / LEA
    if (m == "MOV" || m == "LOAD" || m == "STORE" || m == "LEA") {
        if (instr.operands.size() >= 2) {
            // Reg, Reg.  LOAD/STORE use one register as an ADDRESS ([reg]); ESP then
            // needs a SIB byte and EBP a displacement byte.
            if (instr.operands[0].kind == "REG" && instr.operands[1].kind == "REG") {
                if (m == "LOAD" || m == "STORE") {
                    const string& addr = (m == "LOAD") ? instr.operands[1].value : instr.operands[0].value;
                    return prefixLen + 2 + ((addr == "ESP" || addr == "EBP") ? 1 : 0);
                }
                return prefixLen + 2;
            }
            // Reg, Imm
            if (instr.operands[0].kind == "REG" && instr.operands[1].kind == "IMM") {
                int rSize = getRegisterSize(instr.operands[0].value);
                if (rSize == 8) return 2;
                if (rSize == 16) return 4;
                return 5;
            }
            // Memory with register
            const Operand* memOp = nullptr;
            if (instr.operands[0].kind == "MEM") memOp = &instr.operands[0];
            else if (instr.operands[1].kind == "MEM") memOp = &instr.operands[1];

            if (memOp) {
                int len = prefixLen + 1 + 1 + memExtraLength(*memOp);
                // MOV [mem], imm carries the immediate after the addressing bytes.
                if (m == "MOV" && instr.operands[1].kind == "IMM") len += immFieldBytes(operationBits(instr));
                return len;
            }

            // LOAD/STORE with a label are encoded as MOV reg,[label] / MOV [label],reg:
            // opcode + ModRM + disp32.
            // LEA reg, label is the same: LEA reg,[label] = opcode + ModRM + disp32.
            if ((m == "LOAD" || m == "STORE" || m == "LEA") &&
                (instr.operands[1].kind == "SYM" || instr.operands[0].kind == "SYM")) {
                return prefixLen + 6;
            }
            if (instr.operands[1].kind == "IMM" || instr.operands[1].kind == "SYM") {
                return prefixLen + 5;
            }
            if (instr.operands[0].kind == "IMM" || instr.operands[0].kind == "SYM") {
                return prefixLen + 5;
            }
        }
        return prefixLen + 2;
    }

    if (m == "ADD" || m == "SUB" || m == "CMP" || m == "ADC" || m == "SBB" ||
        m == "AND" || m == "OR" || m == "XOR") {
        if (instr.operands.size() >= 2) {
            if (instr.operands[0].kind == "MEM" || instr.operands[1].kind == "MEM") {
                const Operand& memOp = (instr.operands[0].kind == "MEM") ? instr.operands[0] : instr.operands[1];
                int len = prefixLen + 2 + memExtraLength(memOp);
                if (instr.operands[1].kind == "IMM") {          // [mem], imm
                    int bits = operationBits(instr);
                    len += (bits == 8 || fitsInt8(instr.operands[1].number)) ? 1 : immFieldBytes(bits);
                }
                return len;
            }
            if (instr.operands[1].kind == "REG") {
                return prefixLen + 2;
            }
            if (instr.operands[1].kind == "IMM") {
                int rSize = getRegisterSize(instr.operands[0].value);
                if (rSize == 8) {
                    if (instr.operands[0].value == "AL") return 2;
                    return 3;
                }
                if (fitsInt8(instr.operands[1].number)) return prefixLen + 3;
                if (instr.operands[0].value == "EAX" || instr.operands[0].value == "AX") {
                    return prefixLen + (is16 ? 3 : 5);
                }
                return prefixLen + (is16 ? 4 : 6);
            }
        }
        return prefixLen + 2;
    }

    if (m == "TEST") {
        if (instr.operands.size() >= 2) {
            if (instr.operands[0].kind == "MEM" || instr.operands[1].kind == "MEM") {
                const Operand& memOp = (instr.operands[0].kind == "MEM") ? instr.operands[0] : instr.operands[1];
                int len = prefixLen + 2 + memExtraLength(memOp);
                if (instr.operands[1].kind == "IMM") len += immFieldBytes(operationBits(instr));  // TEST has no imm8 form
                return len;
            }
            if (instr.operands[1].kind == "REG") return prefixLen + 2;
            int rSize = getRegisterSize(instr.operands[0].value);
            if (rSize == 8) {
                if (instr.operands[0].value == "AL") return 2;
                return 3;
            }
            if (instr.operands[0].value == "EAX" || instr.operands[0].value == "AX") {
                return prefixLen + (is16 ? 3 : 5);
            }
            return prefixLen + (is16 ? 4 : 6);
        }
        return prefixLen + 2;
    }

    if (m == "XCHG") {
        if (instr.operands.size() >= 2) {
            if (instr.operands[0].kind == "REG" && instr.operands[1].kind == "REG") {
                if (instr.operands[0].value == "EAX" || instr.operands[1].value == "EAX") return 1;
                if (instr.operands[0].value == "AX" || instr.operands[1].value == "AX") return 2;
                return prefixLen + 2;
            }
            const Operand& memOp = (instr.operands[0].kind == "MEM") ? instr.operands[0] : instr.operands[1];
            int sibLen = hasSIB(memOp) ? 1 : 0;
            int dispLen = getMemoryOperandDisplacementSize(memOp);
            return prefixLen + 2 + sibLen + dispLen;
        }
        return prefixLen + 2;
    }

    if (m == "IMUL") {
        if (instr.operands.size() == 1) {                          // F6/F7 /5 r/m
            int len = prefixLen + 2;
            if (instr.operands[0].kind == "MEM") len += memExtraLength(instr.operands[0]);
            return len;
        }
        const Operand& src = instr.operands[1];
        if (src.kind == "MEM") return prefixLen + 3 + memExtraLength(src);   // 0F AF /r
        if (src.kind == "IMM" && !fitsInt8(src.number))                      // 69 /r id|iw
            return prefixLen + 2 + immFieldBytes(operationBits(instr));
        return prefixLen + 3;                                                // 0F AF /r  or  6B /r ib
    }

    if (isShiftMnemonic(m)) {
        int baseLen = 2;
        if (instr.operands.size() >= 2 && instr.operands[1].kind == "IMM" && instr.operands[1].number != 1) {
            baseLen = 3;
        }
        if (!instr.operands.empty() && instr.operands[0].kind == "MEM") {
            const Operand& memOp = instr.operands[0];
            baseLen += (hasSIB(memOp) ? 1 : 0) + getMemoryOperandDisplacementSize(memOp);
        }
        return prefixLen + baseLen;
    }

    if (m == "INC" || m == "DEC") {
        if (!instr.operands.empty() && instr.operands[0].kind == "REG") {
            int rSize = getRegisterSize(instr.operands[0].value);
            if (rSize == 8) return 2;
            return prefixLen + 1;
        }
        if (!instr.operands.empty() && instr.operands[0].kind == "MEM") {
            const Operand& memOp = instr.operands[0];
            int sibLen = hasSIB(memOp) ? 1 : 0;
            int dispLen = getMemoryOperandDisplacementSize(memOp);
            return prefixLen + 2 + sibLen + dispLen;
        }
        return prefixLen + 1;
    }

    if (m == "PUSH" || m == "POP") {
        if (!instr.operands.empty() && instr.operands[0].kind == "REG") return prefixLen + 1;
        if (!instr.operands.empty() && instr.operands[0].kind == "MEM") {
            const Operand& memOp = instr.operands[0];
            int sibLen = hasSIB(memOp) ? 1 : 0;
            int dispLen = getMemoryOperandDisplacementSize(memOp);
            return prefixLen + 2 + sibLen + dispLen;
        }
        return prefixLen + 1;
    }

    if (m == "NOT" || m == "NEG" || m == "MUL" || m == "DIV" || m == "IDIV") {
        if (!instr.operands.empty() && instr.operands[0].kind == "REG") {
            return prefixLen + 2;
        }
        if (!instr.operands.empty() && instr.operands[0].kind == "MEM") {
            const Operand& memOp = instr.operands[0];
            int sibLen = hasSIB(memOp) ? 1 : 0;
            int dispLen = getMemoryOperandDisplacementSize(memOp);
            return prefixLen + 2 + sibLen + dispLen;
        }
        return prefixLen + 2;
    }

    if (m == "INT") return 2;

    // 16-bit forms carry a 0x66 prefix in 32-bit code: opcode is 2 bytes.
    if (m == "CWD" || m == "CBW" || m == "PUSHA" || m == "POPA" ||
        m == "PUSHF" || m == "POPF" || m == "LODSW" || m == "STOSW" ||
        m == "MOVSW" || m == "CMPSW" || m == "SCASW") return 2;
    if (m == "LODS" || m == "STOS") {            // optional operand only selects the size
        int bits = 32;
        if (instr.operands.size() == 1) {
            const Operand& o = instr.operands[0];
            if (o.kind == "REG") bits = getRegisterSize(o.value);
            else if (o.kind == "MEM" && o.size != 0) bits = o.size;
        }
        return bits == 16 ? 2 : 1;
    }
    if (isZeroOperandMnemonic(m)) return 1;

    if (m == "JMP" || m == "CALL") {
        if (!instr.operands.empty() && instr.operands[0].kind == "REG") return prefixLen + 2;
        if (!instr.operands.empty() && instr.operands[0].kind == "MEM") {
            const Operand& memOp = instr.operands[0];
            return prefixLen + 2 + (hasSIB(memOp) ? 1 : 0) + getMemoryOperandDisplacementSize(memOp);
        }
        return 5;
    }

    if (isLoopMnemonic(m)) return 2;               // opcode + rel8

    if (isBranchMnemonic(m)) return 6;

    return 1;
}

// ---------------------------------------------------------------------
// Function: defineLabel
// Purpose : Binds a parsed label definition to a virtual memory address.
//           Calls addSymbol() and logs a diagnostic error if a duplicate
//           symbol definition is detected.
// @param label   - Label definition metadata (name, line, column)
// @param address - Virtual memory address (current Location Counter LC)
// @param table   - Mutable symbol table map
// @param errors  - Mutable error collection for logging duplicate errors
// ---------------------------------------------------------------------
void defineLabel(const LabelDef& label, unsigned int address,
                 unordered_map<string, Symbol>& table, vector<AsmError>& errors) {
    if (!addSymbol(table, label.name, address, label.line)) {
        addError(errors, "Symbol Error",
                 "duplicate symbol '" + label.name + "' (first defined at line " +
                 to_string(table[label.name].line) + ")",
                 label.line, label.column);
    }
}

// Returns the final value of the Location Counter (= total code size).
// ---------------------------------------------------------------------
// Function: pass1
// Purpose : Executes Pass 1 of the assembler:
//           1. Initializes Location Counter (LC = 0).
//           2. Binds labels preceding each instruction to the current LC.
//           3. Records instruction virtual addresses and computes their exact byte lengths.
//           4. Advances LC by instruction byte length.
//           5. Binds any trailing labels at the end of the file to final LC.
//           6. Returns final LC representing total program byte size.
// @param instructions - Mutable list of Instruction IR nodes (sets address & length)
// @param labels       - List of label definitions extracted during parsing
// @param table        - Mutable symbol table map populated with label addresses
// @param errors       - Mutable error collection for duplicate symbol errors
// @return Final Location Counter value (= total assembled code size in bytes)
// ---------------------------------------------------------------------
unsigned int pass1(vector<Instruction>& instructions, const vector<LabelDef>& labels,
                   unordered_map<string, Symbol>& table, vector<AsmError>& errors) {
    unsigned int LC = 0;      // Location Counter starts at 0
    size_t nextLabel = 0;

    for (size_t i = 0; i < instructions.size(); i++) {
        // labels that sit just before instruction i get the current LC
        while (nextLabel < labels.size() && labels[nextLabel].instrIndex == (int)i) {
            defineLabel(labels[nextLabel], LC, table, errors);
            nextLabel++;
        }
        instructions[i].address = LC;
        instructions[i].length  = getInstructionLength(instructions[i]);
        LC += instructions[i].length;
    }
    // labels at the very end of the file point to the final LC
    while (nextLabel < labels.size()) {
        defineLabel(labels[nextLabel], LC, table, errors);
        nextLabel++;
    }
    return LC;
}

// =====================================================================
//  MAYURI : Pass 2  (resolve forward + backward references)
//  Every SYM operand is looked up in the symbol table built by Pass 1.
//  Because Pass 1 already saw the WHOLE file, a jump to a label defined
//  LATER (forward reference) works exactly like a jump to an earlier one.
//  Undefined symbols are reported with line and column.
//  Still NO encoding: we only store the target address and displacement.
// =====================================================================
// ---------------------------------------------------------------------
// Function: pass2Resolve
// Purpose : Executes Pass 2 reference resolution:
//           1. Looks up target label addresses for all SYM and symbolic MEM operands.
//           2. Flags undefined symbols with line and column numbers.
//           3. Calculates exact relative branch displacements:
//              displacement = targetAddress - (instruction.address + instruction.length).
//           4. Sets resolvedAddress and resolved flag on operands.
// @param instructions - Mutable list of instructions to resolve references for
// @param table        - Populated symbol table from Pass 1
// @param errors       - Mutable error collection for logging undefined symbols
// @return true if all symbols resolved successfully, false if undefined symbols exist
// ---------------------------------------------------------------------
bool pass2Resolve(vector<Instruction>& instructions,
                  const unordered_map<string, Symbol>& table, vector<AsmError>& errors) {
    bool ok = true;

    for (size_t i = 0; i < instructions.size(); i++) {
        Instruction& ins = instructions[i];

        for (size_t k = 0; k < ins.operands.size(); k++) {
            Operand& op = ins.operands[k];
            if (op.kind == "SYM") {
                unordered_map<string, Symbol>::const_iterator it = table.find(op.value);
                if (it == table.end()) {
                    addError(errors, "Symbol Error", "Undefined symbol '" + op.value + "'",
                             ins.line, op.column);
                    ok = false;
                    continue;
                }
                op.resolved        = true;
                op.resolvedAddress = it->second.address;
                // distance from the END of this instruction to the target
                ins.displacement = (int)((long long)it->second.address -
                                         (long long)(ins.address + ins.length));
            } else if (op.kind == "MEM" && !op.symbol.empty()) {
                unordered_map<string, Symbol>::const_iterator it = table.find(op.symbol);
                if (it == table.end()) {
                    addError(errors, "Symbol Error", "Undefined symbol '" + op.symbol + "'",
                             ins.line, op.column);
                    ok = false;
                    continue;
                }
                op.resolved        = true;
                op.resolvedAddress = it->second.address;
            }
        }
    }
    return ok;
}

// =====================================================================
//  DRIVER : runs the whole Worklet 1 pipeline and returns the result
// =====================================================================
// ---------------------------------------------------------------------
// Function: sortErrorsByPosition
// Purpose : Insertion-sorts accumulated diagnostic errors in-place primarily
//           by source line number, and secondarily by column number, guaranteeing
//           deterministic, sequential error reporting.
// @param errors - Mutable error collection to sort
// ---------------------------------------------------------------------
void sortErrorsByPosition(vector<AsmError>& e) {      // simple insertion sort
    for (size_t i = 1; i < e.size(); i++) {
        AsmError key = e[i];
        size_t j = i;
        while (j > 0 && (e[j - 1].line > key.line ||
                        (e[j - 1].line == key.line && e[j - 1].column > key.column))) {
            e[j] = e[j - 1];
            j--;
        }
        e[j] = key;
    }
}

// ---------------------------------------------------------------------
// Function: runWorklet1
// Purpose : Master driver orchestrating the complete Worklet 1 compilation pipeline:
//           - Lexing (tokenizeLine)
//           - Parsing & Label Extraction (parseLine)
//           - Pass 1 Address Allocation & Sizing (pass1)
//           - Pass 2 Reference Resolution (pass2Resolve)
//           - Diagnostic sorting (sortErrorsByPosition)
//           Packages and returns the complete in-memory Worklet1Output struct.
// @param sourceLines - Clean buffered vector of source file lines
// @return Worklet1Output struct containing instructions, symbols, errors, and finalLC
// ---------------------------------------------------------------------
Worklet1Output runWorklet1(const vector<string>& sourceLines) {
    Worklet1Output out;
    vector<LabelDef> labels;

    // Stage 1 + 2: lex and parse every line. A bad line is reported and
    // skipped so that later lines can still be checked.
    for (size_t i = 0; i < sourceLines.size(); i++) {
        size_t errorsBefore = out.errors.size();

        vector<Token> tokens = tokenizeLine(sourceLines[i], (int)i + 1, out.errors);
        if (out.errors.size() != errorsBefore) continue;   // lexer error: skip line

        parseLine(tokens, out.instructions, labels, out.errors);
    }

    // Stage 3: Pass 1  (addresses + symbol table)
    out.finalLC = pass1(out.instructions, labels, out.symbolTable, out.errors);

    // Stage 4: Pass 2  (resolve label references)
    pass2Resolve(out.instructions, out.symbolTable, out.errors);

    sortErrorsByPosition(out.errors);
    out.ok = out.errors.empty();
    return out;
}

// =====================================================================
//  WORKLET 2 / WORKLET 3 ENTRY POINTS  (placeholders for the other teams)
//  These receive Worklet 1's result DIRECTLY as function arguments.
//  They only report what they received; they do not encode or build ELF.
// =====================================================================
// ---------------------------------------------------------------------
// Function: toHex
// Purpose : Formats an unsigned 32-bit integer into a standardized hexadecimal
//           string (e.g. 0x05, 0x1A4) for debug listings and reports.
// @param value - Unsigned integer to format
// @return Formatted hexadecimal string with "0x" prefix
// ---------------------------------------------------------------------
string toHex(unsigned int value) {
    ostringstream out;
    out << "0x";
    if (value < 16) out << "0";
    out << hex << value;
    return out.str();
}

// ---------------------------------------------------------------------
// Function: worklet2_receive
// Purpose : Zero-disk in-memory handoff contract function to Worklet 2 (Binary Encoder).
//           Receives the complete Worklet1Output by const reference, eliminating disk
//           I/O and re-parsing overhead.
// @param ir - Populated Worklet1Output containing sized instructions and operands
// ---------------------------------------------------------------------
void worklet2_receive(const Worklet1Output& ir) {
    cout << "WORKLET 2 INPUT (received by function call)\n";
    cout << "-------------------------\n";
    cout << "Instructions received : " << ir.instructions.size() << "\n";
    cout << "Total code size       : " << ir.finalLC << " bytes\n";
    cout << "[Worklet 2 will encode each instruction here: ModR/M, immediates, ...]\n";
}

// ---------------------------------------------------------------------
// Function: worklet3_receive
// Purpose : Zero-disk in-memory handoff contract function to Worklet 3 (ELF Generator).
//           Receives the resolved symbol table by const reference for generating the
//           ELF .symtab and .strtab sections.
// @param symbolTable - Resolved symbol map containing label names, addresses, and lines
// ---------------------------------------------------------------------
void worklet3_receive(const unordered_map<string, Symbol>& symbolTable) {
    cout << "\nWORKLET 3 INPUT (received by function call)\n";
    cout << "-------------------------\n";
    cout << "Symbols received      : " << symbolTable.size() << "\n";
    cout << "[Worklet 3 will build the ELF symbol table + relocations here]\n";
}

// =====================================================================
//  PRINTING
// =====================================================================
// ---------------------------------------------------------------------
// Function: printErrors
// Purpose : Formats and outputs all collected diagnostic errors to stdout,
//           displaying error category, line, column, and explanatory message.
// @param errors - Collection of sorted AsmError diagnostics
// ---------------------------------------------------------------------
void printErrors(const vector<AsmError>& errors) {
    for (size_t i = 0; i < errors.size(); i++) {
        cout << errors[i].category << " at line " << errors[i].line
             << ", column " << errors[i].column << ": " << errors[i].message << "\n";
    }
    cout << "\n" << errors.size() << " error(s) found. Nothing was handed to Worklet 2.\n";
}

// ---------------------------------------------------------------------
// Function: printIR
// Purpose : Formats and outputs a detailed text listing of the intermediate
//           representation (IR) to stdout, showing instruction indices, mnemonics,
//           operands, virtual addresses, byte lengths, target addresses, and displacements.
// @param instructions - List of sized and resolved instructions
// ---------------------------------------------------------------------
void printIR(const vector<Instruction>& instructions) {
    cout << "INTERMEDIATE REPRESENTATION\n";
    cout << "-------------------------\n";
    for (size_t i = 0; i < instructions.size(); i++) {
        const Instruction& in = instructions[i];
        cout << "Instruction " << (i + 1) << ":\n";
        cout << "Mnemonic: " << in.mnemonic << "\n";
        for (size_t o = 0; o < in.operands.size(); o++) {
            cout << "Operand " << (o + 1) << ": " << in.operands[o].value << "\n";
        }
        cout << "Address: " << toHex(in.address) << "\n";
        cout << "Length: " << in.length << "\n";
        if (!in.operands.empty() && in.operands[0].resolved) {
            cout << "Target Address: " << toHex(in.operands[0].resolvedAddress) << "\n";
            cout << "Displacement: " << in.displacement << "\n";
        }
        if (i + 1 < instructions.size()) cout << "\n";
    }
}

// ---------------------------------------------------------------------
// Function: printSymbolTable
// Purpose : Formats and outputs all defined symbols sorted deterministically
//           by virtual address (then alphabetically by name) to stdout.
// @param table - Symbol table map from Pass 1
// ---------------------------------------------------------------------
void printSymbolTable(const unordered_map<string, Symbol>& table) {
    cout << "SYMBOL TABLE\n";
    cout << "-------------------------\n";
    if (table.empty()) {
        cout << "No symbols defined\n";
        return;
    }
    // copy to a vector and sort by address (then name) so output is deterministic
    vector<Symbol> list;
    for (unordered_map<string, Symbol>::const_iterator it = table.begin(); it != table.end(); ++it)
        list.push_back(it->second);

    for (size_t i = 0; i < list.size(); i++) {         // selection sort
        size_t best = i;
        for (size_t j = i + 1; j < list.size(); j++) {
            if (list[j].address < list[best].address ||
                (list[j].address == list[best].address && list[j].name < list[best].name))
                best = j;
        }
        Symbol temp = list[i];
        list[i]     = list[best];
        list[best]  = temp;
    }
    for (size_t i = 0; i < list.size(); i++) {
        cout << list[i].name << " -> " << toHex(list[i].address) << "\n";
    }
}

// =====================================================================
//  MAIN
// =====================================================================
// ---------------------------------------------------------------------
// Function: readSourceFile
// Purpose : Opens the assembly source file from disk, strips the 3-byte UTF-8
//           Byte Order Mark (BOM) from the first line if present, and buffers
//           all lines into a string vector.
// @param path  - Filesystem path to the assembly source file (.asm)
// @param lines - Output parameter receiving buffered lines
// @return true if file opened and read successfully, false otherwise
// ---------------------------------------------------------------------
bool readSourceFile(const string& path, vector<string>& lines) {
    ifstream file(path.c_str());
    if (!file) return false;
    string line;
    while (getline(file, line)) {
        if (lines.empty() && line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            line = line.substr(3);
        }
        lines.push_back(line);
    }
    return true;
}

// ---------------------------------------------------------------------
// Function: main
// Purpose : Application entry point. Validates command-line arguments, reads
//           the source assembly file, invokes runWorklet1(), and on success
//           prints the IR and symbol table and calls downstream handoffs.
// @param argc - Command-line argument count
// @param argv - Command-line argument strings (argv[1] is input .asm file)
// @return 0 on successful assembly, 1 on file I/O or syntax/semantic errors
// ---------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <file.asm>\n";
        return 1;
    }

    vector<string> sourceLines;
    if (!readSourceFile(argv[1], sourceLines)) {
        cout << "Error: could not open '" << argv[1] << "'\n";
        return 1;
    }

    // ---- Worklet 1 ----
    Worklet1Output result = runWorklet1(sourceLines);

    if (!result.ok) {
        printErrors(result.errors);
        return 1;
    }

    printIR(result.instructions);
    cout << "\n";
    printSymbolTable(result.symbolTable);
    cout << "\nFinal Location Counter: " << toHex(result.finalLC)
         << " (" << result.finalLC << " bytes)\n\n";

    // ---- Handoff by function call (no files) ----
    worklet2_receive(result);
    worklet3_receive(result.symbolTable);

    cout << "\nWorklet 1 completed: validated IR and symbol table passed to "
            "Worklet 2 and Worklet 3 by function call.\n";
    return 0;
}