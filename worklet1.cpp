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
bool isDefined(const unordered_map<string, Symbol>& table, const string& name) {
    return table.find(name) != table.end();
}

// Returns false if the name already exists (duplicate symbol).
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
bool fitsInt8(long long n) {
    if (n > 2147483647LL) n -= 4294967296LL;   // 0xFFFFFFFF is really -1
    return n >= -128 && n <= 127;
}


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

bool hasSIB(const Operand& op) {
    if (!op.indexReg.empty()) return true;
    if (op.baseReg == "ESP") return true;
    return false;
}

// Bytes after the opcode that a memory operand adds: SIB + displacement.
int memExtraLength(const Operand& op) {
    return (hasSIB(op) ? 1 : 0) + getMemoryOperandDisplacementSize(op);
}

// Data size (8/16/32) of the operation: a register operand decides, else a sized
// memory operand, else 32. Mirrors how Worklet 2 resolves it.
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
int immFieldBytes(int bits) { return bits == 8 ? 1 : (bits == 16 ? 2 : 4); }

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

