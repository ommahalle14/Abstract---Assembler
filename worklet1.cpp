// =====================================================================
//  Worklet 1 : Parser, Front-End, and Symbol Table Generator (x86-32)
//  ---------------------------------------------------------------------
//  Produces typed, parsed enum instructions (w2::Instruction) directly.
//  Builds complete ELF symbol table taking addresses from Worklet 2,
//  and directly passes it to Worklet 3.
// =====================================================================
#pragma once

#include "worklet2.h"
#include "worklet3.cpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

// ---------------------------------------------------------------------
// TOKEN AND ERROR DEFINITIONS
// ---------------------------------------------------------------------

enum class TokenType {
    MNEMONIC,
    REGISTER,
    IMMEDIATE,
    IDENTIFIER,
    LABEL,
    COMMA,
    MEMORY
};

struct Token {
    TokenType   type;
    std::string text;
    int         line;
    int         column;
};

struct AsmError {
    std::string category;
    std::string message;
    int         line;
    int         column;
};

enum RefKind {
    REF_REL32,
    REF_ABS_DISP,
    REF_ABS_IMM
};

struct SymRef {
    size_t      instr;
    int         opIdx;
    std::string sym;
    RefKind     kind;
    bool        external;
};

struct LabelEntry {
    std::string name;
    size_t      instrIndex;
    int         line;
    int         column;
};

struct ParsedProgramIR {
    std::vector<w2::Instruction> instructions;
    std::vector<std::string>     instructionText;
    std::vector<int>             instructionLines;
    std::vector<SymRef>          refs;
    std::vector<LabelEntry>      labels;
    w2::LabelId                  endLabel = w2::NO_LABEL;
    uint32_t                     labelCount = 0;
    w2::LabelId                  extId = 0;
    std::vector<AsmError>        errors;
    bool                         ok = false;
};

using Worklet1Output = ParsedProgramIR;

// ---------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------

inline void reportAsmError(std::vector<AsmError>& errors, const std::string& category,
                           const std::string& message, int line, int column) {
    AsmError e;
    e.category = category;
    e.message  = message;
    e.line     = line;
    e.column   = column;
    errors.push_back(e);
}

inline void addError(std::vector<AsmError>& errors, const std::string& category,
                     const std::string& message, int line, int column) {
    reportAsmError(errors, category, message, line, column);
}

inline std::string toUpperString(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    }
    return s;
}

inline std::string toUpper(std::string s) {
    return toUpperString(s);
}

// ---------------------------------------------------------------------
// ENUMERATION & LOOKUP LOGIC
// ---------------------------------------------------------------------

inline bool lookupRegister(const std::string& name, w2::Reg& r, uint8_t& bits) {
    static const char* const n32[8] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
    static const char* const n16[8] = {"AX","CX","DX","BX","SP","BP","SI","DI"};
    static const char* const n8 [8] = {"AL","CL","DL","BL","AH","CH","DH","BH"};
    for (int i = 0; i < 8; ++i) {
        if (name == n32[i]) { r = static_cast<w2::Reg>(i); bits = 32; return true; }
        if (name == n16[i]) { r = static_cast<w2::Reg>(i); bits = 16; return true; }
        if (name == n8 [i]) { r = static_cast<w2::Reg>(i); bits = 8;  return true; }
    }
    return false;
}

inline bool lookupRegSized(const std::string& name, w2::Reg& r, uint8_t& bits) {
    return lookupRegister(name, r, bits);
}

inline bool isSegmentRegister(const std::string& r) {
    return r == "CS" || r == "DS" || r == "SS" || r == "ES" || r == "FS" || r == "GS";
}

inline bool isSegmentReg(const std::string& r) {
    return isSegmentRegister(r);
}

inline bool isValidRegisterName(const std::string& upper) {
    w2::Reg r; uint8_t bits;
    return lookupRegister(upper, r, bits) || isSegmentRegister(upper);
}

inline bool isValidRegister(const std::string& upper) {
    return isValidRegisterName(upper);
}

inline int getRegisterBitWidth(const std::string& upper) {
    w2::Reg r; uint8_t bits;
    if (lookupRegister(upper, r, bits)) return bits;
    if (isSegmentRegister(upper)) return 16;
    return 0;
}

inline int getRegisterSize(const std::string& upper) {
    return getRegisterBitWidth(upper);
}

inline int parseConditionCode(const std::string& s) {
    static const char* const names[][4] = {
        /*0*/ {"O"},  
        /*1*/ {"NO"}, 
        /*2*/ {"B","C","NAE"}, 
        /*3*/ {"AE","NB","NC"},
        /*4*/ {"E","Z"}, /*5*/ {"NE","NZ"}, /*6*/ {"BE","NA"}, /*7*/ {"A","NBE"},
        /*8*/ {"S"},  /*9*/ {"NS"}, /*10*/ {"P","PE"}, /*11*/ {"NP","PO"},
        /*12*/ {"L","NGE"}, /*13*/ {"GE","NL"}, /*14*/ {"LE","NG"}, /*15*/ {"G","NLE"} };
    for (int cc = 0; cc < 16; ++cc)
        for (int k = 0; k < 4 && names[cc][k]; ++k)
            if (s == names[cc][k]) return cc;
    return -1;
}

inline int condCode(const std::string& s) {
    return parseConditionCode(s);
}

inline bool lookupBaseMnemonic(const std::string& m, w2::Op& op) {
    if (m == "MOV")  { op = w2::Op::MOV;  return true; }
    if (m == "ADD")  { op = w2::Op::ADD;  return true; }
    if (m == "SUB")  { op = w2::Op::SUB;  return true; }
    if (m == "CMP")  { op = w2::Op::CMP;  return true; }
    if (m == "INC")  { op = w2::Op::INC;  return true; }
    if (m == "DEC")  { op = w2::Op::DEC;  return true; }
    if (m == "PUSH") { op = w2::Op::PUSH; return true; }
    if (m == "POP")  { op = w2::Op::POP;  return true; }
    if (m == "JMP")  { op = w2::Op::JMP;  return true; }
    if (m == "CALL") { op = w2::Op::CALL; return true; }
    if (m == "JE")   { op = w2::Op::JE;   return true; }
    if (m == "JNE")  { op = w2::Op::JNE;  return true; }
    if (m == "NOP")  { op = w2::Op::NOP;  return true; }
    if (m == "RET")  { op = w2::Op::RET;  return true; }
    if (m == "AND")    { op = w2::Op::AND;    return true; }
    if (m == "OR")     { op = w2::Op::OR;     return true; }
    if (m == "XOR")    { op = w2::Op::XOR;    return true; }
    if (m == "TEST")   { op = w2::Op::TEST;   return true; }
    if (m == "XCHG")   { op = w2::Op::XCHG;   return true; }
    if (m == "IMUL")   { op = w2::Op::IMUL;   return true; }
    if (m == "SHL")    { op = w2::Op::SHL;    return true; }
    if (m == "SHR")    { op = w2::Op::SHR;    return true; }
    if (m == "SAR")    { op = w2::Op::SAR;    return true; }
    if (m == "NOT")    { op = w2::Op::NOT;    return true; }
    if (m == "NEG")    { op = w2::Op::NEG;    return true; }
    if (m == "MUL")    { op = w2::Op::MUL;    return true; }
    if (m == "DIV")    { op = w2::Op::DIV;    return true; }
    if (m == "IDIV")   { op = w2::Op::IDIV;   return true; }
    if (m == "INT")    { op = w2::Op::INT;    return true; }
    if (m == "JL")     { op = w2::Op::JL;     return true; }
    if (m == "JGE")    { op = w2::Op::JGE;    return true; }
    if (m == "JLE")    { op = w2::Op::JLE;    return true; }
    if (m == "JG")     { op = w2::Op::JG;     return true; }
    if (m == "JB")     { op = w2::Op::JB;     return true; }
    if (m == "JAE")    { op = w2::Op::JAE;    return true; }
    if (m == "JBE")    { op = w2::Op::JBE;    return true; }
    if (m == "JA")     { op = w2::Op::JA;     return true; }
    if (m == "JS")     { op = w2::Op::JS;     return true; }
    if (m == "JNS")    { op = w2::Op::JNS;    return true; }
    if (m == "JZ")     { op = w2::Op::JE;     return true; }
    if (m == "JNZ")    { op = w2::Op::JNE;    return true; }
    if (m == "HLT")    { op = w2::Op::HLT;    return true; }
    if (m == "CLC")    { op = w2::Op::CLC;    return true; }
    if (m == "STC")    { op = w2::Op::STC;    return true; }
    if (m == "CLD")    { op = w2::Op::CLD;    return true; }
    if (m == "STD")    { op = w2::Op::STD;    return true; }
    if (m == "CDQ")    { op = w2::Op::CDQ;    return true; }
    if (m == "LEAVE")  { op = w2::Op::LEAVE;  return true; }
    if (m == "PUSHAD") { op = w2::Op::PUSHAD; return true; }
    if (m == "POPAD")  { op = w2::Op::POPAD;  return true; }
    if (m == "PUSHFD") { op = w2::Op::PUSHFD; return true; }
    if (m == "POPFD")  { op = w2::Op::POPFD;  return true; }
    return false;
}

inline bool lookupOp(const std::string& m, w2::Op& op) {
    return lookupBaseMnemonic(m, op);
}

inline bool lookupExtendedMnemonic(const std::string& m, w2::Op& op, uint8_t& cond, uint8_t& size) {
    static const w2::Op jcc[16] = { w2::Op::JO, w2::Op::JNO, w2::Op::JB, w2::Op::JAE,
        w2::Op::JE, w2::Op::JNE, w2::Op::JBE, w2::Op::JA, w2::Op::JS, w2::Op::JNS,
        w2::Op::JP, w2::Op::JNP, w2::Op::JL, w2::Op::JGE, w2::Op::JLE, w2::Op::JG };
    struct E { const char* n; w2::Op op; };
    static const E table[] = {
        {"ADC", w2::Op::ADC}, {"SBB", w2::Op::SBB},
        {"ROL", w2::Op::ROL}, {"ROR", w2::Op::ROR}, {"RCL", w2::Op::RCL}, {"RCR", w2::Op::RCR},
        {"SAL", w2::Op::SHL},
        {"LEA", w2::Op::LEA}, {"MOVZX", w2::Op::MOVZX}, {"MOVSX", w2::Op::MOVSX},
        {"BT", w2::Op::BT}, {"BTS", w2::Op::BTS}, {"BTR", w2::Op::BTR}, {"BTC", w2::Op::BTC},
        {"BSF", w2::Op::BSF}, {"BSR", w2::Op::BSR}, {"BSWAP", w2::Op::BSWAP},
        {"XADD", w2::Op::XADD}, {"CMPXCHG", w2::Op::CMPXCHG},
        {"CBW", w2::Op::CBW}, {"CWDE", w2::Op::CWDE}, {"CWD", w2::Op::CWD},
        {"CMC", w2::Op::CMC}, {"CLI", w2::Op::CLI}, {"STI", w2::Op::STI},
        {"SAHF", w2::Op::SAHF}, {"LAHF", w2::Op::LAHF},
        {"XLAT", w2::Op::XLAT}, {"XLATB", w2::Op::XLAT}, {"WAIT", w2::Op::WAIT},
        {"INT3", w2::Op::INT3}, {"INTO", w2::Op::INTO}, {"IRET", w2::Op::IRET},
        {"UD2", w2::Op::UD2}, {"CPUID", w2::Op::CPUID}, {"RDTSC", w2::Op::RDTSC},
        {"LOOP", w2::Op::LOOP}, {"LOOPE", w2::Op::LOOPE}, {"LOOPZ", w2::Op::LOOPE},
        {"LOOPNE", w2::Op::LOOPNE}, {"LOOPNZ", w2::Op::LOOPNE}, {"JECXZ", w2::Op::JECXZ},
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; ++i)
        if (m == table[i].n) { op = table[i].op; return true; }

    static const struct { const char* stem; w2::Op op; } str[] = {
        {"MOVS", w2::Op::MOVS}, {"CMPS", w2::Op::CMPS}, {"SCAS", w2::Op::SCAS},
        {"LODS", w2::Op::LODS}, {"STOS", w2::Op::STOS} };
    if (m.size() == 5) {
        const char w = m[4];
        for (size_t i = 0; i < sizeof str / sizeof str[0]; ++i)
            if (m.compare(0, 4, str[i].stem) == 0 && (w == 'B' || w == 'W' || w == 'D')) {
                op = str[i].op;  size = (w == 'B') ? 8 : (w == 'W') ? 16 : 32;
                return true;
            }
    }
    int cc;
    if (m.size() > 1 && m[0] == 'J' && (cc = parseConditionCode(m.substr(1))) >= 0)          { op = jcc[cc]; return true; }
    if (m.size() > 3 && m.compare(0, 3, "SET") == 0 && (cc = parseConditionCode(m.substr(3))) >= 0)
        { op = w2::Op::SETCC;  cond = static_cast<uint8_t>(cc); return true; }
    if (m.size() > 4 && m.compare(0, 4, "CMOV") == 0 && (cc = parseConditionCode(m.substr(4))) >= 0)
        { op = w2::Op::CMOVCC; cond = static_cast<uint8_t>(cc); return true; }
    return false;
}

inline bool lookupOpEx(const std::string& m, w2::Op& op, uint8_t& cond, uint8_t& size) {
    return lookupExtendedMnemonic(m, op, cond, size);
}

inline bool isValidMnemonicName(const std::string& upper) {
    if (upper == "LOAD" || upper == "STORE" || upper == "PUSHA" || upper == "POPA" ||
        upper == "PUSHF" || upper == "POPF" || upper == "LODS" || upper == "STOS") return true;
    w2::Op op; uint8_t cond = 0, sz = 0;
    return lookupBaseMnemonic(upper, op) || lookupExtendedMnemonic(upper, op, cond, sz);
}

inline bool isValidMnemonic(const std::string& upper) {
    return isValidMnemonicName(upper);
}

inline bool isBranchInstruction(w2::Op op) {
    switch (op) {
        case w2::Op::JMP: case w2::Op::CALL: case w2::Op::JE: case w2::Op::JNE:
        case w2::Op::JL: case w2::Op::JGE: case w2::Op::JLE: case w2::Op::JG:
        case w2::Op::JB: case w2::Op::JAE: case w2::Op::JBE: case w2::Op::JA:
        case w2::Op::JS: case w2::Op::JNS: case w2::Op::JO: case w2::Op::JNO:
        case w2::Op::JP: case w2::Op::JNP: case w2::Op::LOOP: case w2::Op::LOOPE:
        case w2::Op::LOOPNE: case w2::Op::JECXZ:
            return true;
        default: return false;
    }
}

inline bool isBranchOp(w2::Op op) {
    return isBranchInstruction(op);
}

inline bool isShortBranchInstruction(w2::Op op) {
    return op == w2::Op::LOOP || op == w2::Op::LOOPE || op == w2::Op::LOOPNE || op == w2::Op::JECXZ;
}

inline bool isShortBranchOp(w2::Op op) {
    return isShortBranchInstruction(op);
}

inline bool isImmediateLabelAllowed(w2::Op op) {
    switch (op) {
        case w2::Op::MOV: case w2::Op::PUSH: case w2::Op::ADD: case w2::Op::SUB: case w2::Op::AND:
        case w2::Op::OR: case w2::Op::XOR: case w2::Op::CMP: case w2::Op::ADC: case w2::Op::SBB:
        case w2::Op::TEST:
            return true;
        default: return false;
    }
}

inline bool labelImmediateOk(w2::Op op) {
    return isImmediateLabelAllowed(op);
}

inline bool validateMemoryExpressionSyntax(const std::string& raw, std::string& why) {
    const size_t lb = raw.find('['), rb = raw.rfind(']');
    if (lb == std::string::npos || rb == std::string::npos || rb < lb) return false;
    std::string s;
    for (size_t k = lb + 1; k < rb; ++k) if (!isspace(static_cast<unsigned char>(raw[k]))) s += raw[k];

    int regs = 0, scaled = 0, labels = 0;
    size_t i = 0;
    char sign = '+';
    if (!s.empty() && (s[0] == '+' || s[0] == '-')) { sign = s[0]; i = 1; }
    while (i <= s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] != '+' && s[j] != '-') ++j;
        const std::string term = s.substr(i, j - i);
        if (!term.empty()) {
            const size_t star = term.find('*');
            const std::string name = toUpperString(star == std::string::npos ? term : term.substr(0, star));
            const std::string rhs  = (star == std::string::npos) ? std::string() : toUpperString(term.substr(star + 1));
            w2::Reg r; uint8_t b;
            const bool isReg = lookupRegister(name, r, b) || isSegmentRegister(name);
            const bool rhsReg = !rhs.empty() && (lookupRegister(rhs, r, b) || isSegmentRegister(rhs));
            if (star != std::string::npos && (isReg || rhsReg)) {
                ++regs; ++scaled;
                if (sign == '-') { why = "a scaled index register cannot be subtracted"; return true; }
            } else if (isReg) {
                ++regs;
                if (sign == '-') { why = "a register cannot be subtracted in a memory operand"; return true; }
            } else if (star == std::string::npos && (isalpha(static_cast<unsigned char>(term[0])) || term[0] == '_')) {
                ++labels;
                if (sign == '-') { why = "a label cannot be subtracted in a memory operand"; return true; }
            }
        }
        if (j >= s.size()) break;
        sign = s[j];
        i = j + 1;
    }
    if (scaled > 1) { why = "only one scaled index register is allowed in a memory operand"; return true; }
    if (regs   > 2) { why = "at most two registers (base + index) are allowed in a memory operand"; return true; }
    if (labels > 1) { why = "only one label is allowed in a memory operand"; return true; }
    return false;
}

inline bool memExprProblem(const std::string& raw, std::string& why) {
    return validateMemoryExpressionSyntax(raw, why);
}

// ---------------------------------------------------------------------
// NUMERIC & EXPRESSION PARSERS
// ---------------------------------------------------------------------

inline bool parseNumericLiteral(const std::string& text, long long& value) {
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
    } else if (end - i >= 2 && isdigit(static_cast<unsigned char>(text[i]))) {
        char sfx = static_cast<char>(tolower(static_cast<unsigned char>(text[end - 1])));
        if      (sfx == 'h')               { base = 16; end--; }
        else if (sfx == 'b')               { base = 2;  end--; }
        else if (sfx == 'o' || sfx == 'q') { base = 8;  end--; }
        else if (sfx == 'd')               { base = 10; end--; }
    }

    long long result = 0;
    for (; i < end; i++) {
        char c = text[i];
        int digit;
        if (c >= '0' && c <= '9')                     digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        if (digit >= base) return false;

        result = result * base + digit;
        if (result > 4294967295LL) return false;
    }
    if (negative) {
        result = -result;
        if (result < -2147483648LL) return false;
    }
    value = result;
    return true;
}

inline bool parseNumber(const std::string& text, long long& value) {
    return parseNumericLiteral(text, value);
}

inline bool parseMemoryExpressionComponents(const std::string& inside,
                                           std::string& baseReg, std::string& indexReg, int& scale,
                                           long long& displacement, std::string& symbol) {
    std::string str;
    for (size_t k = 0; k < inside.size(); k++) {
        if (!isspace(static_cast<unsigned char>(inside[k]))) str += inside[k];
    }
    if (str.empty()) return false;

    std::vector<std::string> terms;
    std::vector<char> ops;
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
        std::string term = terms[t];
        char sign = ops[t];
        long long mult = (sign == '-') ? -1 : 1;

        size_t starPos = term.find('*');
        if (starPos != std::string::npos) {
            std::string p1 = term.substr(0, starPos);
            std::string p2 = term.substr(starPos + 1);
            std::string rPart, sPart;
            if (isValidRegisterName(toUpperString(p1))) {
                rPart = toUpperString(p1);
                sPart = p2;
            } else if (isValidRegisterName(toUpperString(p2))) {
                rPart = toUpperString(p2);
                sPart = p1;
            } else {
                return false;
            }
            long long scVal = 1;
            if (!parseNumericLiteral(sPart, scVal) || (scVal != 1 && scVal != 2 && scVal != 4 && scVal != 8)) {
                return false;
            }
            indexReg = rPart;
            scale = static_cast<int>(scVal);
            continue;
        }

        std::string upperTerm = toUpperString(term);
        if (isValidRegisterName(upperTerm)) {
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

        long long numVal = 0;
        if (parseNumericLiteral(term, numVal)) {
            displacement += mult * numVal;
            continue;
        }

        if (isalpha(static_cast<unsigned char>(term[0])) || term[0] == '_') {
            symbol = term;
            continue;
        }

        return false;
    }
    return true;
}

inline bool parseMemoryExpression(const std::string& inside,
                                  std::string& baseReg, std::string& indexReg, int& scale,
                                  long long& displacement, std::string& symbol) {
    return parseMemoryExpressionComponents(inside, baseReg, indexReg, scale, displacement, symbol);
}

// ---------------------------------------------------------------------
// LEXER
// ---------------------------------------------------------------------

inline std::vector<Token> tokenizeSourceLine(const std::string& line, int lineNumber, std::vector<AsmError>& errors) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < line.size()) {
        char c = line[i];
        int column = static_cast<int>(i) + 1;

        if (isspace(static_cast<unsigned char>(c))) { i++; continue; }
        if (c == ';') break;

        if (c == ',') {
            tokens.push_back({TokenType::COMMA, ",", lineNumber, column});
            i++;
            continue;
        }

        if (c == '[') {
            size_t start = i;
            int bracketColumn = column;
            i++;
            while (i < line.size() && line[i] != ']' && line[i] != ';') i++;
            if (i < line.size() && line[i] == ']') {
                i++;
                std::string memText = line.substr(start, i - start);
                tokens.push_back({TokenType::MEMORY, memText, lineNumber, bracketColumn});
            } else {
                reportAsmError(errors, "Lexical Error", "missing closing ']' for memory operand",
                               lineNumber, bracketColumn);
            }
            continue;
        }

        if (isdigit(static_cast<unsigned char>(c)) ||
            ((c == '-' || c == '+') && i + 1 < line.size() && isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t start = i;
            i++;
            while (i < line.size() && (isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
            std::string text = line.substr(start, i - start);

            long long ignored;
            if (parseNumericLiteral(text, ignored)) {
                tokens.push_back({TokenType::IMMEDIATE, text, lineNumber, column});
            } else {
                reportAsmError(errors, "Lexical Error", "invalid immediate '" + text + "'",
                               lineNumber, column);
            }
            continue;
        }

        if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < line.size() && (isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
            std::string word  = line.substr(start, i - start);
            std::string upper = toUpperString(word);

            bool followedByColon = (i < line.size() && line[i] == ':');

            if (followedByColon) {
                i++;
                if (isValidMnemonicName(upper) || isValidRegisterName(upper)) {
                    reportAsmError(errors, "Lexical Error",
                                   "label name '" + word + "' is a reserved word", lineNumber, column);
                } else {
                    tokens.push_back({TokenType::LABEL, word, lineNumber, column});
                }
            } else if (isValidMnemonicName(upper)) {
                tokens.push_back({TokenType::MNEMONIC, upper, lineNumber, column});
            } else if (isValidRegisterName(upper)) {
                tokens.push_back({TokenType::REGISTER, upper, lineNumber, column});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, word, lineNumber, column});
            }
            continue;
        }

        reportAsmError(errors, "Lexical Error", std::string("invalid character '") + c + "'",
                       lineNumber, column);
        i++;
    }
    return tokens;
}

inline std::vector<Token> tokenizeLine(const std::string& line, int lineNumber, std::vector<AsmError>& errors) {
    return tokenizeSourceLine(line, lineNumber, errors);
}

// ---------------------------------------------------------------------
// WORKLET 1 MAIN ENTRY POINT (PARSER & FRONT-END)
// ---------------------------------------------------------------------

inline void sortErrorsBySourcePosition(std::vector<AsmError>& e) {
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

inline void sortErrorsByPosition(std::vector<AsmError>& e) {
    sortErrorsBySourcePosition(e);
}

inline ParsedProgramIR parseAssemblySource(const std::vector<std::string>& sourceLines,
                                          const std::set<std::string>* externSet = nullptr) {
    std::cout << "[Worklet 1: parseAssemblySource] Tokenizing and analyzing " << sourceLines.size() << " assembly source lines...\n";

    ParsedProgramIR out;
    std::set<std::string> externs;
    if (externSet != nullptr) {
        externs = *externSet;
    }

    struct LineTokens {
        int lineNum;
        std::string rawLine;
        std::vector<Token> tokens;
    };
    std::vector<LineTokens> lines;
    lines.reserve(sourceLines.size());

    for (size_t i = 0; i < sourceLines.size(); ++i) {
        size_t errsBefore = out.errors.size();
        std::vector<Token> toks = tokenizeSourceLine(sourceLines[i], static_cast<int>(i + 1), out.errors);
        if (out.errors.size() != errsBefore) continue;
        lines.push_back({ static_cast<int>(i + 1), sourceLines[i], toks });
    }

    struct ParsedInstrLine {
        size_t instrIndex;
        int lineNum;
        std::string rawText;
        std::vector<Token> tokens;
    };
    std::vector<ParsedInstrLine> instrLines;
    std::unordered_map<std::string, int> definedLabelLine;
    std::vector<LabelEntry> labelDefs;

    for (size_t l = 0; l < lines.size(); ++l) {
        const auto& lt = lines[l];
        if (lt.tokens.empty()) continue;

        size_t idx = 0;
        while (idx < lt.tokens.size() && lt.tokens[idx].type == TokenType::LABEL) {
            const std::string& name = lt.tokens[idx].text;
            int line = lt.tokens[idx].line;
            int col  = lt.tokens[idx].column;
            if (definedLabelLine.count(name)) {
                reportAsmError(out.errors, "Symbol Error",
                               "duplicate symbol '" + name + "' (first defined at line " +
                               std::to_string(definedLabelLine[name]) + ")", line, col);
            } else {
                definedLabelLine[name] = line;
                LabelEntry le;
                le.name = name;
                le.instrIndex = instrLines.size();
                le.line = line;
                le.column = col;
                labelDefs.push_back(le);
            }
            idx++;
        }

        if (idx < lt.tokens.size()) {
            ParsedInstrLine pil;
            pil.instrIndex = instrLines.size();
            pil.lineNum = lt.lineNum;
            pil.rawText = lt.rawLine;
            pil.tokens.assign(lt.tokens.begin() + idx, lt.tokens.end());
            instrLines.push_back(pil);
        }
    }

    std::cout << "[Worklet 1: parseAssemblySource] Discovered " << labelDefs.size() << " label definitions across "
              << instrLines.size() << " instructions.\n";

    // Dense label ID assignment
    std::map<size_t, w2::LabelId> idByInstr;
    for (size_t k = 0; k < labelDefs.size(); ++k) {
        idByInstr[labelDefs[k].instrIndex] = 0;
    }
    w2::LabelId nextId = 0;
    for (auto& kv : idByInstr) {
        kv.second = nextId++;
    }
    std::unordered_map<std::string, w2::LabelId> idByName;
    for (size_t k = 0; k < labelDefs.size(); ++k) {
        idByName[labelDefs[k].name] = idByInstr[labelDefs[k].instrIndex];
    }
    out.extId = nextId++;
    out.labelCount = nextId;
    out.endLabel = w2::NO_LABEL;
    if (idByInstr.count(instrLines.size())) {
        out.endLabel = idByInstr[instrLines.size()];
    }

    std::cout << "[Worklet 1: parseAssemblySource] Assigned " << out.labelCount << " dense label IDs (extId=" << out.extId << ").\n";

    // Parse each instruction directly into w2::Instruction
    out.instructions.reserve(instrLines.size());
    out.instructionText.reserve(instrLines.size());
    out.instructionLines.reserve(instrLines.size());

    for (size_t i = 0; i < instrLines.size(); ++i) {
        const auto& pil = instrLines[i];
        const auto& toks = pil.tokens;
        int line = pil.lineNum;

        w2::Instruction inst = w2::Instruction();
        inst.defLabel = w2::NO_LABEL;
        if (idByInstr.count(pil.instrIndex)) {
            inst.defLabel = idByInstr[pil.instrIndex];
        }

        if (toks[0].type != TokenType::MNEMONIC && toks[0].type != TokenType::IDENTIFIER) {
            reportAsmError(out.errors, "Syntax Error", "expected a mnemonic, found '" + toks[0].text + "'",
                           line, toks[0].column);
            continue;
        }

        std::string m = toUpperString(toks[0].text);
        w2::Op curOp = w2::Op::NOP;
        bool isLoad = (m == "LOAD");
        bool isStore = (m == "STORE");
        bool isLods = (m == "LODS");
        bool isStos = (m == "STOS");
        bool isStackAll = (m == "PUSHA" || m == "POPA" || m == "PUSHF" || m == "POPF");

        if (isLoad || isStore) {
            inst.op = curOp = w2::Op::MOV;
        } else if (isStackAll) {
            inst.op = curOp = (m == "PUSHA") ? w2::Op::PUSHAD : (m == "POPA") ? w2::Op::POPAD
                            : (m == "PUSHF") ? w2::Op::PUSHFD : w2::Op::POPFD;
            inst.prefix = 0x66;
            inst.operandCount = 0;
            out.instructions.push_back(inst);
            out.instructionText.push_back(pil.rawText);
            out.instructionLines.push_back(line);
            continue;
        } else if (isLods || isStos) {
            inst.op = curOp = (isLods) ? w2::Op::LODS : w2::Op::STOS;
        } else if (!lookupBaseMnemonic(m, inst.op) && !lookupExtendedMnemonic(m, inst.op, inst.cond, inst.size)) {
            reportAsmError(out.errors, "Syntax Error", "unsupported mnemonic '" + toks[0].text + "'",
                           line, toks[0].column);
            continue;
        } else {
            curOp = inst.op;
        }

        // Parse operands
        struct RawParsedOp {
            TokenType type;
            std::string text;
            int col;
            int explicitSize;
            w2::Operand op;
            bool ok;
        };
        std::vector<RawParsedOp> parsedOps;

        size_t tIdx = 1;
        while (tIdx < toks.size()) {
            int explicitSize = 0;
            int startCol = toks[tIdx].column;
            bool symIsMemory = (isLoad && parsedOps.size() == 1) ||
                               (isStore && parsedOps.size() == 0) ||
                               (m == "LEA" && parsedOps.size() == 1);

            if (toks[tIdx].type == TokenType::IDENTIFIER) {
                std::string u = toUpperString(toks[tIdx].text);
                if (u == "BYTE" || u == "WORD" || u == "DWORD" || u == "QWORD") {
                    if (u == "BYTE") explicitSize = 8;
                    else if (u == "WORD") explicitSize = 16;
                    else if (u == "DWORD") explicitSize = 32;
                    else if (u == "QWORD") explicitSize = 64;
                    tIdx++;
                    if (tIdx < toks.size() && toks[tIdx].type == TokenType::IDENTIFIER &&
                        toUpperString(toks[tIdx].text) == "PTR") {
                        tIdx++;
                    }
                } else if (u == "PTR") {
                    tIdx++;
                }
            }

            if (tIdx >= toks.size()) {
                reportAsmError(out.errors, "Syntax Error", "missing operand after size directive",
                               line, toks.back().column + static_cast<int>(toks.back().text.size()));
                break;
            }

            const Token& curr = toks[tIdx];
            RawParsedOp rpo;
            rpo.type = curr.type;
            rpo.text = curr.text;
            rpo.col = startCol;
            rpo.explicitSize = explicitSize;
            rpo.ok = false;

            if (curr.type == TokenType::REGISTER) {
                w2::Reg r; uint8_t bits;
                if (!lookupRegister(toUpperString(curr.text), r, bits)) {
                    reportAsmError(out.errors, "Syntax Error", "unknown register '" + curr.text + "'", line, startCol);
                } else {
                    if (symIsMemory) {
                        if (bits != 32) {
                            reportAsmError(out.errors, "Syntax Error", "16/8-bit register '" + curr.text + "' cannot be used as an address (use 32-bit)", line, startCol);
                        } else {
                            rpo.op = w2::memOp(r, w2::Reg::NONE, 1, 0);
                            rpo.ok = true;
                        }
                    } else {
                        rpo.op = w2::regOp(r, bits);
                        rpo.ok = true;
                    }
                }
            } else if (curr.type == TokenType::IMMEDIATE) {
                long long numVal = 0;
                parseNumericLiteral(curr.text, numVal);
                rpo.op = w2::immOp(static_cast<int32_t>(static_cast<uint32_t>(numVal)));
                rpo.ok = true;
            } else if (curr.type == TokenType::IDENTIFIER) {
                const std::string sym = curr.text;
                w2::LabelId lId = 0;
                bool isExt = false;
                if (idByName.count(sym)) {
                    lId = idByName[sym];
                    isExt = false;
                } else if (externs.count(sym)) {
                    lId = out.extId;
                    isExt = true;
                } else {
                    reportAsmError(out.errors, "Symbol Error", "undefined symbol '" + sym + "'", line, startCol);
                }

                int opIdx = static_cast<int>(parsedOps.size());
                if (symIsMemory) {
                    rpo.op = w2::labelMemOp(lId);
                    SymRef sr; sr.instr = pil.instrIndex; sr.opIdx = opIdx; sr.sym = sym; sr.kind = REF_ABS_DISP; sr.external = isExt;
                    out.refs.push_back(sr);
                    rpo.ok = true;
                } else if (isBranchInstruction(curOp)) {
                    if (isExt && isShortBranchInstruction(curOp)) {
                        reportAsmError(out.errors, "Syntax Error", m + " cannot reference an external symbol (rel8)", line, startCol);
                    } else {
                        rpo.op = w2::labelOp(lId);
                        if (isExt) {
                            SymRef sr; sr.instr = pil.instrIndex; sr.opIdx = opIdx; sr.sym = sym; sr.kind = REF_REL32; sr.external = true;
                            out.refs.push_back(sr);
                        }
                        rpo.ok = true;
                    }
                } else if (isImmediateLabelAllowed(curOp)) {
                    rpo.op = w2::immOp(0x7FFFFFFF);
                    SymRef sr; sr.instr = pil.instrIndex; sr.opIdx = opIdx; sr.sym = sym; sr.kind = REF_ABS_IMM; sr.external = isExt;
                    out.refs.push_back(sr);
                    rpo.ok = true;
                } else {
                    reportAsmError(out.errors, "Syntax Error", "a label cannot be used as an operand of " + m, line, startCol);
                }
            } else if (curr.type == TokenType::MEMORY) {
                std::string why;
                if (validateMemoryExpressionSyntax(curr.text, why)) {
                    reportAsmError(out.errors, "Syntax Error", why + " ('" + curr.text + "')", line, startCol);
                } else {
                    std::string memInside = (curr.text.size() >= 2) ? curr.text.substr(1, curr.text.size() - 2) : "";
                    std::string baseReg, indexReg, symbol;
                    int scale = 1;
                    long long displacement = 0;
                    if (!parseMemoryExpressionComponents(memInside, baseReg, indexReg, scale, displacement, symbol)) {
                        reportAsmError(out.errors, "Syntax Error", "invalid memory expression '" + curr.text + "'", line, startCol);
                    } else if (displacement < -2147483648LL || displacement > 4294967295LL) {
                        reportAsmError(out.errors, "Syntax Error", "memory displacement out of 32-bit range in '" + curr.text + "'", line, startCol);
                    } else {
                        const int32_t disp = static_cast<int32_t>(static_cast<uint32_t>(displacement));
                        w2::Reg base = w2::Reg::NONE, index = w2::Reg::NONE;
                        bool regOk = true;
                        if (!baseReg.empty()) {
                            uint8_t b;
                            if (!lookupRegister(toUpperString(baseReg), base, b) || b != 32) {
                                reportAsmError(out.errors, "Syntax Error", "invalid base register '" + baseReg + "'", line, startCol);
                                regOk = false;
                            }
                        }
                        if (!indexReg.empty()) {
                            uint8_t b;
                            if (!lookupRegister(toUpperString(indexReg), index, b) || b != 32) {
                                reportAsmError(out.errors, "Syntax Error", "invalid index register '" + indexReg + "'", line, startCol);
                                regOk = false;
                            }
                        }
                        if (regOk) {
                            if (index == w2::Reg::ESP && scale == 1 && base != w2::Reg::NONE && base != w2::Reg::ESP) {
                                index = base; base = w2::Reg::ESP;
                            }
                            const uint8_t sc = static_cast<uint8_t>(scale ? scale : 1);
                            const uint8_t bits = static_cast<uint8_t>(explicitSize);
                            int opIdx = static_cast<int>(parsedOps.size());

                            if (!symbol.empty()) {
                                w2::LabelId lId = 0;
                                bool isExt = false;
                                if (idByName.count(symbol)) {
                                    lId = idByName[symbol];
                                    isExt = false;
                                } else if (externs.count(symbol)) {
                                    lId = out.extId;
                                    isExt = true;
                                } else {
                                    reportAsmError(out.errors, "Symbol Error", "undefined symbol '" + symbol + "'", line, startCol);
                                }
                                rpo.op = w2::memLabelOp(base, index, sc, disp, lId, bits);
                                SymRef sr; sr.instr = pil.instrIndex; sr.opIdx = opIdx; sr.sym = symbol; sr.kind = REF_ABS_DISP; sr.external = isExt;
                                out.refs.push_back(sr);
                                rpo.ok = true;
                            } else {
                                rpo.op = w2::memOp(base, index, sc, disp, bits);
                                rpo.ok = true;
                            }
                        }
                    }
                }
            } else {
                reportAsmError(out.errors, "Syntax Error", "expected an operand, found '" + curr.text + "'", line, startCol);
            }

            parsedOps.push_back(rpo);
            tIdx++;

            if (tIdx < toks.size()) {
                if (toks[tIdx].type != TokenType::COMMA) {
                    reportAsmError(out.errors, "Syntax Error", "expected comma", line, toks[tIdx].column);
                    break;
                }
                tIdx++;
                if (tIdx >= toks.size()) {
                    reportAsmError(out.errors, "Syntax Error", "missing operand after comma", line, toks.back().column + 1);
                    break;
                }
            }
        }

        // Handle LODS / STOS optional width
        if (isLods || isStos) {
            unsigned bits = 32;
            if (parsedOps.size() == 1) {
                const auto& po = parsedOps[0];
                if (po.type == TokenType::REGISTER) {
                    bits = po.op.size ? po.op.size : 32;
                } else if (po.type == TokenType::MEMORY && po.op.size != 0) {
                    bits = po.op.size;
                }
            }
            if (bits != 8 && bits != 16 && bits != 32) {
                reportAsmError(out.errors, "Syntax Error", m + " operand size must be 8, 16 or 32 bits", line, pil.tokens[0].column);
                continue;
            }
            inst.op = (isLods) ? w2::Op::LODS : w2::Op::STOS;
            inst.size = static_cast<uint8_t>(bits);
            inst.operandCount = 0;
            out.instructions.push_back(inst);
            out.instructionText.push_back(pil.rawText);
            out.instructionLines.push_back(line);
            continue;
        }

        // Handle LOAD / STORE
        if (isLoad || isStore) {
            if (parsedOps.size() != 2) {
                reportAsmError(out.errors, "Syntax Error", m + " needs 2 operands", line, pil.tokens[0].column);
                continue;
            }
            inst.op = w2::Op::MOV;
            inst.operands[0] = parsedOps[0].op;
            inst.operands[1] = parsedOps[1].op;
            inst.operandCount = 2;
            out.instructions.push_back(inst);
            out.instructionText.push_back(pil.rawText);
            out.instructionLines.push_back(line);
            continue;
        }

        // Normal instructions
        if (parsedOps.size() > 3) {
            reportAsmError(out.errors, "Syntax Error", "more than 3 operands", line, pil.tokens[0].column);
            continue;
        }

        inst.operandCount = static_cast<uint8_t>(parsedOps.size());
        for (size_t k = 0; k < parsedOps.size(); ++k) {
            inst.operands[k] = parsedOps[k].op;
        }

        out.instructions.push_back(inst);
        out.instructionText.push_back(pil.rawText);
        out.instructionLines.push_back(line);
    }

    sortErrorsBySourcePosition(out.errors);
    out.labels = labelDefs;
    out.ok = out.errors.empty();

    std::cout << "[Worklet 1: parseAssemblySource] Parsing complete: generated " << out.instructions.size()
              << " IR instructions, recorded " << out.refs.size() << " symbol references ("
              << out.errors.size() << " errors).\n";

    return out;
}

inline ParsedProgramIR runWorklet1(const std::vector<std::string>& sourceLines,
                                  const std::set<std::string>* externSet = nullptr) {
    return parseAssemblySource(sourceLines, externSet);
}

// ---------------------------------------------------------------------
// FILE I/O
// ---------------------------------------------------------------------

inline bool readSourceFileLines(const std::string& path, std::vector<std::string>& lines) {
    std::ifstream file(path.c_str());
    if (!file) return false;
    std::string line;
    while (std::getline(file, line)) {
        if (lines.empty() && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }
        lines.push_back(line);
    }
    return true;
}

inline bool readSourceFile(const std::string& path, std::vector<std::string>& lines) {
    return readSourceFileLines(path, lines);
}

// =============================================================================
// WORKLET 1: Symbol Table Generation
// Takes label definitions from Worklet 1, exact byte addresses from Worklet 2
// (instrOffsets), and symbols from Worklet 3 (srcSyms), generates the complete
// ELF Symbol Table (.symtab & .strtab), and passes it directly to Worklet 3.
// =============================================================================

inline void symWriteU8(std::vector<uint8_t>& buf, uint8_t val) {
    buf.push_back(val);
}

inline void symWriteU16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

inline void symWriteU32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

inline bool buildSymbolTable(w3::ObjectModel& model,
                            const ParsedProgramIR& w1,
                            const std::vector<size_t>& instrOffsets,
                            w3::SourceSymbols& srcSyms,
                            std::string& errorMsg) {
    std::cout << "[Worklet 1: buildSymbolTable] Resolving " << w1.labels.size()
              << " text label address(es) using Worklet 2 instruction offsets...\n";

    // Step 1: Assign exact text offsets from Worklet 2 to all W1 label definitions
    for (size_t i = 0; i < w1.labels.size(); ++i) {
        size_t idx = w1.labels[i].instrIndex;
        uint32_t addr = (idx < instrOffsets.size()) ? static_cast<uint32_t>(instrOffsets[idx]) : 0u;
        srcSyms.labels[w1.labels[i].name] = { w3::SEC_TEXT, addr };
    }

    bool hasSymbols = !srcSyms.labels.empty() || !srcSyms.globals.empty() ||
                      !srcSyms.externs.empty() || !srcSyms.equConstants.empty() ||
                      !model.relocs.empty();
    if (!hasSymbols) {
        std::cout << "[Worklet 1: buildSymbolTable] No user symbols found; skipping .symtab/.strtab generation.\n";
        return true;
    }

    std::cout << "[Worklet 1: buildSymbolTable] Building .symtab for "
              << srcSyms.labels.size() << " label(s), "
              << srcSyms.externs.size() << " extern(s), "
              << srcSyms.globals.size() << " global(s), "
              << srcSyms.equConstants.size() << " equ constant(s)...\n";

    // Validate that all declared globals are actually defined
    for (const std::string& g : srcSyms.globals) {
        if (srcSyms.labels.count(g) == 0 && srcSyms.equConstants.count(g) == 0) {
            int line = srcSyms.globalLines.count(g) ? srcSyms.globalLines.at(g) : 0;
            if (line > 0) {
                errorMsg = "Symbol Error at line " + std::to_string(line) + ": Global symbol '" + g + "' is declared but not defined";
            } else {
                errorMsg = "Global symbol '" + g + "' is declared but not defined";
            }
            return false;
        }
    }

    // 1. Separate symbols into LOCAL and GLOBAL/EXTERN groups
    std::set<std::string> globalSet(srcSyms.globals.begin(), srcSyms.globals.end());
    // _start is implicitly global if present
    if (srcSyms.labels.count("_start")) {
        globalSet.insert("_start");
    }

    std::vector<w3::Sym> localSyms;
    std::vector<w3::Sym> nonLocalSyms;

    // Normal labels defined in sections
    std::vector<std::string> sortedLabels;
    for (const auto& kv : srcSyms.labels) {
        sortedLabels.push_back(kv.first);
    }
    std::sort(sortedLabels.begin(), sortedLabels.end());

    for (const std::string& name : sortedLabels) {
        const auto& info = srcSyms.labels.at(name);
        w3::Sym s = w3::Sym();
        s.name  = name;
        s.value = info.offset;
        std::string secName = (info.section == w3::SEC_TEXT) ? ".text" : (info.section == w3::SEC_DATA) ? ".data" : ".bss";
        int sIdx = w3::sectionIndex(model, secName);
        s.shndx = (sIdx >= 0) ? static_cast<uint16_t>(sIdx) : w3::SHN_UNDEF;
        s.size  = 0;

        if (globalSet.count(name)) {
            s.bind = w3::STB_GLOBAL;
            s.type = (secName == ".text") ? w3::STT_FUNC : w3::STT_OBJECT;
            nonLocalSyms.push_back(s);
        } else {
            s.bind = w3::STB_LOCAL;
            s.type = (secName == ".text") ? w3::STT_FUNC : w3::STT_OBJECT;
            localSyms.push_back(s);
        }
    }

    // equ constants (SHN_ABS)
    std::vector<std::string> sortedEqus;
    for (const auto& kv : srcSyms.equConstants) {
        sortedEqus.push_back(kv.first);
    }
    std::sort(sortedEqus.begin(), sortedEqus.end());

    for (const std::string& name : sortedEqus) {
        w3::Sym s = w3::Sym();
        s.name  = name;
        s.value = srcSyms.equConstants.at(name);
        s.shndx = w3::SHN_ABS;
        s.size  = 0;
        if (globalSet.count(name)) {
            s.bind = w3::STB_GLOBAL;
            s.type = w3::STT_NOTYPE;
            nonLocalSyms.push_back(s);
        } else {
            s.bind = w3::STB_LOCAL;
            s.type = w3::STT_NOTYPE;
            localSyms.push_back(s);
        }
    }

    // extern symbols (SHN_UNDEF, STB_GLOBAL)
    std::vector<std::string> sortedExterns = srcSyms.externs;
    std::sort(sortedExterns.begin(), sortedExterns.end());
    for (const std::string& name : sortedExterns) {
        w3::Sym s = w3::Sym();
        s.name  = name;
        s.value = 0;
        s.shndx = w3::SHN_UNDEF;
        s.bind  = w3::STB_GLOBAL;
        s.type  = w3::STT_NOTYPE;
        s.size  = 0;
        nonLocalSyms.push_back(s);
    }

    // 2. Populate model.symbols (Rule 2)
    model.symbols.clear();
    w3::Sym nullSym = w3::Sym();
    model.symbols.push_back(nullSym);

    for (size_t s = 1; s < model.sections.size(); ++s) {
        if (model.sections[s].type == w3::SHT_PROGBITS || model.sections[s].type == w3::SHT_NOBITS) {
            w3::Sym ss = w3::Sym();
            ss.name  = model.sections[s].name;
            ss.shndx = static_cast<uint16_t>(s);
            ss.bind  = w3::STB_LOCAL;
            ss.type  = w3::STT_SECTION;
            model.symbols.push_back(ss);
        }
    }

    for (const auto& s : localSyms) {
        model.symbols.push_back(s);
    }
    for (const auto& s : nonLocalSyms) {
        model.symbols.push_back(s);
    }

    // 3. Build .strtab
    w3::Section strtab = w3::Section();
    strtab.name    = ".strtab";
    strtab.type    = w3::SHT_STRTAB;
    strtab.flags   = 0;
    strtab.align   = 1;
    strtab.entsize = 0;

    strtab.data.push_back(0);

    std::vector<uint32_t> stNameOffsets(model.symbols.size(), 0);
    for (size_t i = 1; i < model.symbols.size(); ++i) {
        const w3::Sym& sym = model.symbols[i];
        if (sym.type == w3::STT_SECTION) {
            stNameOffsets[i] = 0;
        } else {
            stNameOffsets[i] = static_cast<uint32_t>(strtab.data.size());
            for (char c : sym.name) {
                strtab.data.push_back(static_cast<uint8_t>(c));
            }
            strtab.data.push_back(0);
        }
    }

    // 4. Build .symtab binary payload
    w3::Section symtab = w3::Section();
    symtab.name     = ".symtab";
    symtab.type     = w3::SHT_SYMTAB;
    symtab.flags    = 0;
    symtab.align    = 4;
    symtab.entsize  = 16;
    symtab.linkName = ".strtab";
    symtab.info     = w3::firstNonLocalIndex(model);

    for (size_t i = 0; i < model.symbols.size(); ++i) {
        const w3::Sym& sym = model.symbols[i];
        symWriteU32(symtab.data, stNameOffsets[i]);
        symWriteU32(symtab.data, sym.value);
        symWriteU32(symtab.data, sym.size);
        symWriteU8(symtab.data, w3::makeSymInfo(sym.bind, sym.type));
        symWriteU8(symtab.data, 0);
        symWriteU16(symtab.data, sym.shndx);
    }

    model.sections.push_back(symtab);
    model.sections.push_back(strtab);

    std::cout << "[Worklet 1: buildSymbolTable] .symtab built with "
              << (symtab.data.size() / 16) << " symbol entries ("
              << symtab.data.size() << " bytes), .strtab "
              << strtab.data.size() << " bytes; passed directly to Worklet 3.\n";

    return true;
}

// ---------------------------------------------------------------------
// WORKLET 1 OUTPUT DISPLAY HELPER
// ---------------------------------------------------------------------
inline void printParsedProgramIR(const ParsedProgramIR& ir) {
    std::cout << "\n======================================================================\n";
    std::cout << "WORKLET 1 OUTPUT: Parsed Program IR & Discovered Symbols\n";
    std::cout << "======================================================================\n";
    std::cout << "Total Instructions  : " << ir.instructions.size() << "\n";
    std::cout << "Total Defined Labels: " << ir.labels.size() << "\n";
    std::cout << "Dense Label Count   : " << ir.labelCount << " (extId=" << ir.extId << ")\n";
    std::cout << "Symbol References   : " << ir.refs.size() << "\n";
    std::cout << "Status              : " << (ir.ok ? "SUCCESS (0 errors)" : "FAILED") << "\n\n";

    if (!ir.labels.empty()) {
        std::cout << "Discovered Labels:\n";
        for (size_t i = 0; i < ir.labels.size(); ++i) {
            std::cout << "  [" << i << "] Label '" << ir.labels[i].name
                      << "' at instruction index " << ir.labels[i].instrIndex
                      << " (line " << ir.labels[i].line << ")\n";
        }
        std::cout << "\n";
    }

    if (!ir.instructions.empty()) {
        std::cout << "Parsed IR Instructions (w2::Instruction):\n";
        for (size_t i = 0; i < ir.instructions.size(); ++i) {
            std::cout << "  [" << std::setw(3) << i << "] Line "
                      << std::setw(3) << ir.instructionLines[i] << ": "
                      << std::left << std::setw(30) << ir.instructionText[i]
                      << " (operands=" << static_cast<int>(ir.instructions[i].operandCount);
            if (ir.instructions[i].defLabel != w2::NO_LABEL) {
                std::cout << ", defLabel=" << ir.instructions[i].defLabel;
            }
            std::cout << ")\n";
        }
        std::cout << std::right;
    }
    std::cout << "======================================================================\n\n";
}

#ifdef WORKLET1_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file.asm>\n";
        return 1;
    }
    std::vector<std::string> lines;
    if (!readSourceFileLines(argv[1], lines)) {
        std::cerr << "Error: could not read file " << argv[1] << "\n";
        return 1;
    }
    std::cout << "=== Worklet 1 Standalone Runner ===\n";
    ParsedProgramIR ir = parseAssemblySource(lines);
    printParsedProgramIR(ir);
    return ir.ok ? 0 : 1;
}
#endif
