#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <cstdio>
#include <iomanip>


// Ingest Worklet 2 encoder interface
#include "worklet2.h"

namespace w3 {


// =============================================================================
// SECTION 1: ELF32 Structures and Constants (formerly elf_format.h)
// =============================================================================
// OWNER: Member 2 - ELF32 Structs

#include <cstdint>


// ELF Header (Elf32_Ehdr) - 52 bytes
struct Elf32_Ehdr {
    uint8_t  e_ident[16]; // Magic number and other info (EI_MAG0..3, EI_CLASS, etc.)
    uint16_t e_type;      // Object file type: ET_REL = 1
    uint16_t e_machine;   // Architecture: EM_386 = 3
    uint32_t e_version;   // Object file version: EV_CURRENT = 1
    uint32_t e_entry;     // Entry point virtual address (0 for ET_REL)
    uint32_t e_phoff;     // Program header table file offset (0 for ET_REL)
    uint32_t e_shoff;     // Section header table file offset
    uint32_t e_flags;     // Processor-specific flags (0 for x86)
    uint16_t e_ehsize;    // ELF header size in bytes (52)
    uint16_t e_phentsize; // Program header table entry size (0)
    uint16_t e_phnum;     // Program header table entry count (0)
    uint16_t e_shentsize; // Section header table entry size (40)
    uint16_t e_shnum;     // Section header table entry count
    uint16_t e_shstrndx;  // Section name string table index
};

// Section Header (Elf32_Shdr) - 40 bytes
struct Elf32_Shdr {
    uint32_t sh_name;      // Section name (index into .shstrtab)
    uint32_t sh_type;      // Section type (SHT_PROGBITS, SHT_SYMTAB, etc.)
    uint32_t sh_flags;     // Section flags (SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR)
    uint32_t sh_addr;      // Section virtual addr at execution (0 in ET_REL)
    uint32_t sh_offset;    // Section file offset in bytes
    uint32_t sh_size;      // Section size in bytes
    uint32_t sh_link;      // Link to another section (resolved index)
    uint32_t sh_info;      // Additional section information
    uint32_t sh_addralign; // Section address alignment boundary
    uint32_t sh_entsize;   // Entry size if section holds fixed-size entries
};

// Symbol Table Entry (Elf32_Sym) - 16 bytes
struct Elf32_Sym {
    uint32_t st_name;  // Symbol name (index into .strtab)
    uint32_t st_value; // Symbol value (offset or absolute value)
    uint32_t st_size;  // Symbol size in bytes
    uint8_t  st_info;  // Symbol type and binding attribute (STB_*, STT_*)
    uint8_t  st_other; // Symbol visibility (0 = default)
    uint16_t st_shndx; // Section index (SHN_UNDEF, SHN_ABS, or section number)
};

// Relocation Entry (Elf32_Rel) - 8 bytes
struct Elf32_Rel {
    uint32_t r_offset; // Byte offset within section where relocation applies
    uint32_t r_info;   // Symbol table index and relocation type: (sym << 8) | type
};

// Common ELF identification constants
constexpr uint8_t EI_MAG0       = 0;
constexpr uint8_t EI_MAG1       = 1;
constexpr uint8_t EI_MAG2       = 2;
constexpr uint8_t EI_MAG3       = 3;
constexpr uint8_t EI_CLASS      = 4;
constexpr uint8_t EI_DATA       = 5;
constexpr uint8_t EI_VERSION    = 6;

constexpr uint8_t ELFMAG0       = 0x7F;
constexpr uint8_t ELFMAG1       = 'E';
constexpr uint8_t ELFMAG2       = 'L';
constexpr uint8_t ELFMAG3       = 'F';

constexpr uint8_t ELFCLASS32    = 1;
constexpr uint8_t ELFDATA2LSB   = 1; // 2's complement, little endian
constexpr uint32_t EV_CURRENT   = 1;
constexpr uint16_t ET_REL       = 1; // Relocatable file
constexpr uint16_t EM_386       = 3; // Intel 80386



// =============================================================================
// SECTION 2: Shared Object Model & Helpers (formerly w3.h)
// =============================================================================
// =============================================================================
// OWNER: Member 4 - Shared Contract (FROZEN on Day 1; change only by group vote)
//
// Everything Worklet 3 passes around lives in ONE struct: ObjectModel.
// Each member fills in only their own pieces:
//
//   Member 1 : sections[SEC_DATA].data / sections[SEC_BSS].sizeIfNoBits,
//              and the label/extern/global info that Member 3 turns into symbols
//   (Worklet 2 via the driver) : sections[SEC_TEXT].data
//   Member 3 : model.symbols  and the ".symtab" / ".strtab" sections
//   Member 4 : model.relocs   and the ".rel.text" / ".rel.data" sections
//   Member 2 : reads the finished model, adds ".shstrtab", lays out the file
//
// -----------------------------------------------------------------------------
// RULE 1 - SECTION INDEX RULE
//   model.sections[i] IS ELF section number i.  No hidden shifting.
//     0 = null section (all zero, empty name)
//     1 = .text    2 = .data    3 = .bss        <- ALWAYS present, even if empty
//   After those, in this order, only if used:
//     .symtab, .strtab, .rel.text, .rel.data, .shstrtab (.shstrtab added last by M2)
//   Because 1..3 never move, Sym.shndx for a normal label is simply SEC_TEXT,
//   SEC_DATA or SEC_BSS.  For any other section, use sectionIndex(model, name).
//
// RULE 2 - SYMBOL INDEX RULE
//   model.symbols[i] IS ELF symbol number i.  The vector is already in final
//   .symtab order, so there is no "+1 for the null entry" anywhere else:
//     symbols[0]            = null symbol (all zero, empty name)
//     symbols[1..3]         = one STT_SECTION symbol per section 1..3 (LOCAL)
//     then                  = other LOCAL symbols (ordinary labels)
//     then                  = GLOBAL symbols and undefined externs (SHN_UNDEF)
//   ELF requires every LOCAL symbol to come before every non-LOCAL one, and
//   .symtab's sh_info must be firstNonLocalIndex(model).
//   For a STT_SECTION symbol, Sym.name holds the SECTION NAME (".text" ...) so
//   it can be looked up, but the serialised st_name field must be 0.
//
// RULE 3 - ONE PLACE FOR INDEX ARITHMETIC
//   Nobody computes a section or symbol index by hand.  Use the helpers at the
//   bottom of this file:
//       sectionIndex(model, ".data")        symbolIndex(model, "msg")
//       makeSymInfo(bind, type)             makeRelInfo(symIdx, relType)
//   symtab.cpp (M3) and reloc.cpp (M4) MUST both use symbolIndex().
//
// RULE 4 - ENDIANNESS / PADDING
//   ELF bytes are written one byte at a time, little-endian (see M2's writer).
//   Never fwrite() a C++ struct: padding and host endianness would corrupt it.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>


// ---- ELF constants used by more than one member (single definition) ---------
// elf_format.h (Member 2) only defines the Elf32_* STRUCTS and includes this file.

// Section types (sh_type)
constexpr uint32_t SHT_NULL     = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB   = 2;
constexpr uint32_t SHT_STRTAB   = 3;
constexpr uint32_t SHT_NOBITS   = 8;    // .bss: occupies memory, no file bytes
constexpr uint32_t SHT_REL      = 9;    // i386 uses REL (addend stored in the code), not RELA

// Section flags (sh_flags)
constexpr uint32_t SHF_WRITE     = 0x1;
constexpr uint32_t SHF_ALLOC     = 0x2;
constexpr uint32_t SHF_EXECINSTR = 0x4;

// Special section indices (st_shndx)
constexpr uint16_t SHN_UNDEF = 0;       // undefined / extern symbol
constexpr uint16_t SHN_ABS   = 0xFFF1;  // absolute value (equ constants)

// Symbol binding / type (st_info = (bind << 4) | type)
constexpr uint8_t STB_LOCAL  = 0;
constexpr uint8_t STB_GLOBAL = 1;
constexpr uint8_t STB_WEAK   = 2;
constexpr uint8_t STT_NOTYPE  = 0;
constexpr uint8_t STT_OBJECT  = 1;
constexpr uint8_t STT_FUNC    = 2;
constexpr uint8_t STT_SECTION = 3;

// i386 relocation types
constexpr uint8_t R_386_32   = 1;       // S + A        (absolute 32-bit address)
constexpr uint8_t R_386_PC32 = 2;       // S + A - P    (relative, e.g. call extern)

// Fixed section numbers (Rule 1)
constexpr uint16_t SEC_NULL = 0;
constexpr uint16_t SEC_TEXT = 1;
constexpr uint16_t SEC_DATA = 2;
constexpr uint16_t SEC_BSS  = 3;

// ---- The model --------------------------------------------------------------

struct Section {
    std::string          name;          // ".text", ".data", ".bss", ".symtab", ...
    uint32_t             type;          // SHT_*
    uint32_t             flags;         // SHF_*
    uint32_t             align;         // sh_addralign (1, 4, 16 ...)
    uint32_t             entsize;       // 16 for .symtab, 8 for .rel.*, else 0
    std::vector<uint8_t> data;          // payload (empty for SHT_NOBITS)
    uint32_t             sizeIfNoBits;  // size of .bss (only used for SHT_NOBITS)
    std::string          linkName;      // sh_link target by NAME (".strtab", ".symtab"); M2 resolves to an index
    uint32_t             info;          // sh_info: .symtab -> first non-local symbol index;
                                        //          .rel.*  -> index of the section being relocated
};

struct Sym {
    std::string name;     // symbol name; for STT_SECTION symbols: the section name (Rule 2)
    uint32_t    value;    // offset inside its section, or the constant for SHN_ABS
    uint16_t    shndx;    // SHN_UNDEF, SHN_ABS, or a section number
    uint8_t     bind;     // STB_*
    uint8_t     type;     // STT_*
    uint32_t    size;     // 0 if unknown
};

struct Reloc {
    uint32_t    offset;   // byte offset inside `section` where the 4-byte field lives
    std::string symName;  // symbol being referenced (looked up with symbolIndex)
    uint8_t     type;     // R_386_32 or R_386_PC32
    std::string section;  // section being patched: ".text" or ".data"
    uint32_t    line = 0; // source line for error reporting
    uint32_t    col  = 0; // source column for error reporting
};

struct ObjectModel {
    std::vector<Section> sections;   // Rule 1
    std::vector<Sym>     symbols;    // Rule 2
    std::vector<Reloc>   relocs;
};

// ---- Shared helpers (header-only, the ONLY place index arithmetic lives) -----

// Returns the section number for `name`, or -1 if there is no such section.
inline int sectionIndex(const ObjectModel& m, const std::string& name) {
    for (size_t i = 0; i < m.sections.size(); ++i)
        if (m.sections[i].name == name) return static_cast<int>(i);
    return -1;
}

// Returns the ELF symbol number for `name`, or -1 if not found.
// Because symbols[i] IS symbol i (Rule 2), the vector position is the answer.
inline int symbolIndex(const ObjectModel& m, const std::string& name) {
    for (size_t i = 1; i < m.symbols.size(); ++i)        // skip the null entry
        if (m.symbols[i].name == name) return static_cast<int>(i);
    return -1;
}

// Index of the first non-LOCAL symbol; this is .symtab's sh_info.
// Returns symbols.size() if every symbol is local.
inline uint32_t firstNonLocalIndex(const ObjectModel& m) {
    for (size_t i = 0; i < m.symbols.size(); ++i)
        if (m.symbols[i].bind != STB_LOCAL) return static_cast<uint32_t>(i);
    return static_cast<uint32_t>(m.symbols.size());
}

// st_info = (bind << 4) | (type & 0xF)
inline uint8_t makeSymInfo(uint8_t bind, uint8_t type) {
    return static_cast<uint8_t>((bind << 4) | (type & 0x0F));
}

// r_info = (symbol index << 8) | relocation type
inline uint32_t makeRelInfo(uint32_t symIdx, uint8_t relType) {
    return (symIdx << 8) | relType;
}

// Creates the fixed skeleton required by Rules 1 and 2:
//   sections: null, .text, .data, .bss        symbols: null, 3 section symbols
// The driver calls this once at the start; members then fill things in.
inline ObjectModel makeEmptyModel() {
    ObjectModel m;

    Section nul = Section();                 // zero-initialised
    m.sections.push_back(nul);

    Section text = Section();
    text.name = ".text"; text.type = SHT_PROGBITS;
    text.flags = SHF_ALLOC | SHF_EXECINSTR; text.align = 16;
    m.sections.push_back(text);

    return m;
}



// =============================================================================
// SECTION 3: Section Manager & Location Counters (formerly sections.h & sections.cpp)
// =============================================================================
// OWNER: Member 1 - Section Location Counters & Directives Data
// Accumulates bytes and location counters for .data and .bss sections.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


class SectionManager {
public:
    std::vector<uint8_t> dataBytes;
    uint32_t bssSize;

    SectionManager() : bssSize(0) {}

    uint32_t currentDataOffset() const { return static_cast<uint32_t>(dataBytes.size()); }
    uint32_t currentBssOffset() const { return bssSize; }

    void emitByte(uint8_t b) { dataBytes.push_back(b); }
    void emitWord(uint16_t w) {
        dataBytes.push_back(static_cast<uint8_t>(w & 0xFF));
        dataBytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    }
    void emitDword(uint32_t d) {
        dataBytes.push_back(static_cast<uint8_t>(d & 0xFF));
        dataBytes.push_back(static_cast<uint8_t>((d >> 8) & 0xFF));
        dataBytes.push_back(static_cast<uint8_t>((d >> 16) & 0xFF));
        dataBytes.push_back(static_cast<uint8_t>((d >> 24) & 0xFF));
    }
    void emitBytes(const std::vector<uint8_t>& bs) {
        dataBytes.insert(dataBytes.end(), bs.begin(), bs.end());
    }

    void reserveBss(uint32_t bytes) { bssSize += bytes; }
};


// OWNER: Member 1 - Section Location Counters & Directives Data
// Implementations for section management helpers.

    // SectionManager methods are inline in sections.h for efficiency and simplicity.


// =============================================================================
// SECTION 4: Expression Evaluator Engine (formerly expr.h & expr.cpp)
// =============================================================================
// OWNER: Member 3 - Expression Evaluator
// Evaluates numeric expressions: integers, +, -, *, /, (), unary -, $, symbols, equ constants, label differences.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


struct ExprContext {
    uint32_t currentLC; // Value of '$'
    const std::unordered_map<std::string, uint32_t>* equConstants; // equ name -> value
    const std::unordered_map<std::string, uint32_t>* labelAddresses; // label name -> address
};

// Evaluates an expression string. Returns true on success, false on syntax or undefined symbol error.
bool evalExpression(const std::string& exprStr, const ExprContext& ctx, int line, int col,
                    int32_t& outVal, std::string& errorMsg);


// OWNER: Member 3 - Expression Evaluator
// Recursive descent evaluator for integers (dec/hex/char), +, -, *, /, (), unary -, $, symbols, equ constants.

#include <cctype>
#include <sstream>


namespace {

enum class TokType {
    END, INT, IDENT, PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, DOLLAR
};

struct Token {
    TokType type;
    int32_t val;
    std::string text;
    int col;
};

class Lexer {
    std::string s;
    size_t pos;
    int baseCol;

public:
    Lexer(const std::string& str, int column) : s(str), pos(0), baseCol(column) {}

    bool next(Token& tok, std::string& err) {
        while (pos < s.size() && isspace(static_cast<unsigned char>(s[pos]))) {
            pos++;
        }
        if (pos >= s.size()) {
            tok = { TokType::END, 0, "", baseCol + static_cast<int>(pos) };
            return true;
        }

        int col = baseCol + static_cast<int>(pos);
        char c = s[pos];

        if (c == '+') { pos++; tok = { TokType::PLUS, 0, "+", col }; return true; }
        if (c == '-') { pos++; tok = { TokType::MINUS, 0, "-", col }; return true; }
        if (c == '*') { pos++; tok = { TokType::STAR, 0, "*", col }; return true; }
        if (c == '/') { pos++; tok = { TokType::SLASH, 0, "/", col }; return true; }
        if (c == '(') { pos++; tok = { TokType::LPAREN, 0, "(", col }; return true; }
        if (c == ')') { pos++; tok = { TokType::RPAREN, 0, ")", col }; return true; }
        if (c == '$') { pos++; tok = { TokType::DOLLAR, 0, "$", col }; return true; }

        // Character literal e.g. 'A'
        if (c == '\'') {
            pos++;
            if (pos >= s.size()) {
                err = "Unterminated character literal";
                return false;
            }
            char ch = s[pos++];
            if (pos >= s.size() || s[pos] != '\'') {
                err = "Invalid character literal";
                return false;
            }
            pos++;
            tok = { TokType::INT, static_cast<int32_t>(static_cast<unsigned char>(ch)), "", col };
            return true;
        }

        // Number: decimal or hex (0x...)
        if (isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos;
            int base = 10;
            if (c == '0' && pos + 1 < s.size() && (s[pos + 1] == 'x' || s[pos + 1] == 'X')) {
                base = 16;
                pos += 2;
            }
            uint64_t val = 0;
            size_t digitStart = pos;
            while (pos < s.size() && (isxdigit(static_cast<unsigned char>(s[pos])))) {
                char d = s[pos];
                int digit = 0;
                if (isdigit(static_cast<unsigned char>(d))) digit = d - '0';
                else if (base == 16 && d >= 'a' && d <= 'f') digit = d - 'a' + 10;
                else if (base == 16 && d >= 'A' && d <= 'F') digit = d - 'A' + 10;
                else break;
                val = val * base + digit;
                if (val > 0xFFFFFFFFULL) {
                    err = "Number overflow";
                    return false;
                }
                pos++;
            }
            if (pos == digitStart && base == 16) {
                err = "Expected hex digits after '0x'";
                return false;
            }
            tok = { TokType::INT, static_cast<int32_t>(static_cast<uint32_t>(val)), s.substr(start, pos - start), col };
            return true;
        }

        // Identifier
        if (isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.') {
            size_t start = pos;
            while (pos < s.size() && (isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '_' || s[pos] == '.')) {
                pos++;
            }
            std::string id = s.substr(start, pos - start);
            tok = { TokType::IDENT, 0, id, col };
            return true;
        }

        err = std::string("Unexpected character in expression: '") + c + "'";
        return false;
    }
};

class Parser {
    Lexer& lex;
    Token curr;
    const ExprContext& ctx;
    std::string err;
    int line;
    int depth;

    bool advance() {
        return lex.next(curr, err);
    }

public:
    Parser(Lexer& l, const ExprContext& c, int lnum) : lex(l), ctx(c), line(lnum), depth(0) {}

    bool parse(int32_t& out) {
        if (!advance()) return false;
        if (curr.type == TokType::END) {
            err = "Empty expression";
            return false;
        }
        if (!expr(out)) return false;
        if (curr.type != TokType::END) {
            err = "Extra tokens after expression";
            return false;
        }
        return true;
    }

    const std::string& getError() const { return err; }

private:
    bool expr(int32_t& out) {
        if (!term(out)) return false;
        while (curr.type == TokType::PLUS || curr.type == TokType::MINUS) {
            TokType op = curr.type;
            if (!advance()) return false;
            int32_t right = 0;
            if (!term(right)) return false;
            if (op == TokType::PLUS) out += right;
            else out -= right;
        }
        return true;
    }

    bool term(int32_t& out) {
        if (!factor(out)) return false;
        while (curr.type == TokType::STAR || curr.type == TokType::SLASH) {
            TokType op = curr.type;
            int col = curr.col;
            if (!advance()) return false;
            int32_t right = 0;
            if (!factor(right)) return false;
            if (op == TokType::STAR) {
                out *= right;
            } else {
                if (right == 0) {
                    std::ostringstream ss;
                    ss << "Division by zero at line " << line << ", column " << col;
                    err = ss.str();
                    return false;
                }
                out /= right;
            }
        }
        return true;
    }

    bool factor(int32_t& out) {
        if (curr.type == TokType::PLUS || curr.type == TokType::MINUS) {
            int col = curr.col;
            if (++depth > 256) {
                std::ostringstream ss;
                ss << "expression nested too deeply at line " << line << ", column " << col;
                err = ss.str();
                return false;
            }
            TokType op = curr.type;
            if (!advance()) return false;
            if (!factor(out)) return false;
            depth--;
            if (op == TokType::MINUS) out = -out;
            return true;
        }
        if (curr.type == TokType::LPAREN) {
            int col = curr.col;
            if (++depth > 256) {
                std::ostringstream ss;
                ss << "expression nested too deeply at line " << line << ", column " << col;
                err = ss.str();
                return false;
            }
            if (!advance()) return false;
            if (!expr(out)) return false;
            depth--;
            if (curr.type != TokType::RPAREN) {
                err = "Missing closing ')'";
                return false;
            }
            return advance();
        }
        if (curr.type == TokType::INT) {
            out = curr.val;
            return advance();
        }
        if (curr.type == TokType::DOLLAR) {
            out = static_cast<int32_t>(ctx.currentLC);
            return advance();
        }
        if (curr.type == TokType::IDENT) {
            std::string name = curr.text;
            int col = curr.col;
            if (ctx.equConstants && ctx.equConstants->count(name)) {
                out = static_cast<int32_t>(ctx.equConstants->at(name));
            } else if (ctx.labelAddresses && ctx.labelAddresses->count(name)) {
                out = static_cast<int32_t>(ctx.labelAddresses->at(name));
            } else {
                std::ostringstream ss;
                ss << "Undefined symbol '" << name << "' in expression at line " << line << ", column " << col;
                err = ss.str();
                return false;
            }
            return advance();
        }

        err = "Expected number, symbol or '('";
        return false;
    }
};

} // namespace

bool evalExpression(const std::string& exprStr, const ExprContext& ctx, int line, int col,
                    int32_t& outVal, std::string& errorMsg) {
    Lexer lex(exprStr, col);
    Parser parser(lex, ctx, line);
    if (!parser.parse(outVal)) {
        errorMsg = parser.getError();
        return false;
    }
    return true;
}



// =============================================================================
// SECTION 5: Symbol Metadata Definitions (formerly symtab.h)
// =============================================================================
// OWNER: Member 3 - Symbol Table & String Table Construction

#include <string>
#include <unordered_map>
#include <vector>


struct SourceSymbols {
    // Labels defined in source (name -> {sectionIndex, offset})
    struct LabelInfo {
        uint16_t section;
        uint32_t offset;
    };
    std::unordered_map<std::string, LabelInfo> labels;
    std::vector<std::string> globals;
    std::unordered_map<std::string, int> globalLines;
    std::vector<std::string> externs;
    std::unordered_map<std::string, uint32_t> equConstants;
};

// Builds model.symbols and appends .symtab and .strtab sections to model.sections.
// Follows Rule 2:
//   symbols[0] = null symbol
//   symbols[1..3] = section symbols (.text, .data, .bss)
//   then STB_LOCAL symbols
//   then STB_GLOBAL and SHN_UNDEF externs
bool buildSymtab(ObjectModel& model, const SourceSymbols& srcSyms, std::string& errorMsg);



// =============================================================================
// SECTION 6: Directives Parser & Source Splitter (formerly directives.h & directives.cpp)
// =============================================================================
// OWNER: Member 1 - Source Splitter & Directives Parser
// Splits raw source into: .text lines (preserving line numbers), data bytes, data symbols, and global/extern lists.

#include <string>
#include <vector>


struct SourceSplitResult {
    std::vector<std::string> textLines;      // .text instruction lines (1-indexed matching original line numbers)
    SourceSymbols            symbols;        // Data & text labels, globals, externs, equ constants
    SectionManager           sections;       // .data bytes and .bss size
    std::vector<Reloc>       dataRelocs;     // Relocations needed within .data (e.g. from dd label)
    bool                     ok;
    std::vector<std::string> errors;
};

// Splits input source lines into sections and processes all directives:
// section .text|.data|.bss, global, extern, equ, db, dw, dd, resb, resw, resd, times N db X.
SourceSplitResult splitSource(const std::vector<std::string>& lines);


// OWNER: Member 1 - Source Splitter & Directives Parser
// Handles section switching (.text, .data, .bss), global, extern, equ, db, dw, dd, resb/w/d, times.

#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>


namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string toUpperStr(std::string s) {
    for (char& c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

// Strips comments starting with ';' unless inside quotes
std::string stripComments(const std::string& s) {
    std::string res;
    bool inQuotes = false;
    char quoteChar = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (inQuotes) {
            res.push_back(c);
            if (c == quoteChar) inQuotes = false;
        } else {
            if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
                res.push_back(c);
            } else if (c == ';') {
                break;
            } else {
                res.push_back(c);
            }
        }
    }
    return res;
}

// Splits comma-separated values, respecting quotes and parentheses
std::vector<std::string> splitCommaArgs(const std::string& s) {
    std::vector<std::string> args;
    std::string cur;
    bool inQuotes = false;
    char quoteChar = 0;
    int parenDepth = 0;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (inQuotes) {
            cur.push_back(c);
            if (c == quoteChar) inQuotes = false;
        } else {
            if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
                cur.push_back(c);
            } else if (c == '(') {
                parenDepth++;
                cur.push_back(c);
            } else if (c == ')') {
                parenDepth--;
                cur.push_back(c);
            } else if (c == ',' && parenDepth == 0) {
                args.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
    }
    if (!cur.empty() || !args.empty()) {
        args.push_back(trim(cur));
    }
    return args;
}

// Parses string of format: <identifier> [('+' | '-') <integer>]
// Returns true if matched, filling sym and addend.
bool parseSymbolPlusAddend(const std::string& str, std::string& sym, int32_t& addend) {
    std::string s = trim(str);
    if (s.empty()) return false;
    size_t i = 0;
    if (!isalpha(static_cast<unsigned char>(s[i])) && s[i] != '_' && s[i] != '.') {
        return false;
    }
    size_t idStart = i;
    while (i < s.size() && (isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '.')) {
        i++;
    }
    sym = s.substr(idStart, i - idStart);
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i == s.size()) {
        addend = 0;
        return true;
    }
    if (s[i] != '+' && s[i] != '-') {
        return false;
    }
    char sign = s[i++];
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i == s.size()) return false;
    int base = 10;
    if (s[i] == '0' && i + 1 < s.size() && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        base = 16;
        i += 2;
    }
    size_t digitStart = i;
    uint64_t val = 0;
    while (i < s.size() && isxdigit(static_cast<unsigned char>(s[i]))) {
        char d = s[i];
        int digit = 0;
        if (isdigit(static_cast<unsigned char>(d))) digit = d - '0';
        else if (base == 16 && d >= 'a' && d <= 'f') digit = d - 'a' + 10;
        else if (base == 16 && d >= 'A' && d <= 'F') digit = d - 'A' + 10;
        else break;
        val = val * base + digit;
        i++;
    }
    if (i == digitStart) return false;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i != s.size()) return false;
    addend = (sign == '-') ? -static_cast<int32_t>(val) : static_cast<int32_t>(val);
    return true;
}

// Parses TIMES directive arguments: count expression and directive keyword + values
bool parseTimesLine(const std::string& lineAfterTimes,
                    std::string& countStr,
                    std::string& subDir,
                    std::string& valStr) {
    std::istringstream iss(lineAfterTimes);
    std::string word;
    size_t currentPos = 0;
    while (iss >> word) {
        std::string u = toUpperStr(word);
        if (u == "DB" || u == "DW" || u == "DD" || u == "RESB" || u == "RESW" || u == "RESD") {
            size_t wordPos = lineAfterTimes.find(word, currentPos);
            countStr = trim(lineAfterTimes.substr(0, wordPos));
            subDir = u;
            valStr = trim(lineAfterTimes.substr(wordPos + word.size()));
            return !countStr.empty();
        }
        size_t found = lineAfterTimes.find(word, currentPos);
        if (found != std::string::npos) {
            currentPos = found + word.size();
        }
    }
    return false;
}

// Parses string as unsigned 64-bit number or evaluates expression
bool parseUnsigned64OrExpr(const std::string& s, const ExprContext& ctx, int line, int col, uint64_t& outVal, std::string& err) {
    std::string trimmed = trim(s);
    if (!trimmed.empty() && (isdigit(static_cast<unsigned char>(trimmed[0])) || (trimmed.size() > 2 && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X')))) {
        char* endp = nullptr;
        errno = 0;
        unsigned long long v = strtoull(trimmed.c_str(), &endp, 0);
        if (errno == 0 && endp != nullptr && *endp == '\0') {
            outVal = static_cast<uint64_t>(v);
            return true;
        }
    }
    int32_t val32 = 0;
    if (!evalExpression(trimmed, ctx, line, col, val32, err)) {
        return false;
    }
    if (val32 < 0) {
        err = "negative reservation count " + std::to_string(val32);
        return false;
    }
    outVal = static_cast<uint64_t>(val32);
    return true;
}

} // namespace

SourceSplitResult splitSource(const std::vector<std::string>& lines) {
    std::cout << "[Worklet 3: splitSource] Processing " << lines.size() << " source lines...\n";
    SourceSplitResult res;
    res.ok = true;
    res.textLines.resize(lines.size()); // keep line numbers exact (1-based index matching)

    uint16_t curSection = SEC_TEXT; // Default section is .text

    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        int lineNum = static_cast<int>(lineIdx + 1);
        std::string raw = lines[lineIdx];
        std::string clean = stripComments(raw);
        clean = trim(clean);

        if (clean.empty()) {
            res.textLines[lineIdx] = "";
            continue;
        }

        // Check for equ definition: <name> equ <expr>
        // Also: <name>: equ <expr>
        {
            std::istringstream iss(clean);
            std::string firstWord;
            iss >> firstWord;
            std::string candidateName = firstWord;
            if (!candidateName.empty() && candidateName.back() == ':') {
                candidateName.pop_back();
            }
            std::string nextWord;
            iss >> nextWord;
            if (toUpperStr(nextWord) == "EQU") {
                if (res.symbols.equConstants.count(candidateName) > 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": duplicate equ definition of '" + candidateName + "'");
                    res.ok = false;
                    res.textLines[lineIdx] = "";
                    continue;
                }
                if (res.symbols.labels.count(candidateName) > 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": symbol '" + candidateName + "' already defined as a label");
                    res.ok = false;
                    res.textLines[lineIdx] = "";
                    continue;
                }
                std::string exprPart;
                std::getline(iss, exprPart);
                exprPart = trim(exprPart);

                ExprContext ctx;
                ctx.currentLC = (curSection == SEC_DATA) ? res.sections.currentDataOffset()
                              : (curSection == SEC_BSS) ? res.sections.currentBssOffset() : 0;
                ctx.equConstants = &res.symbols.equConstants;
                
                std::unordered_map<std::string, uint32_t> labelAddrs;
                for (const auto& kv : res.symbols.labels) {
                    labelAddrs[kv.first] = kv.second.offset;
                }
                ctx.labelAddresses = &labelAddrs;

                int32_t val = 0;
                std::string err;
                if (!evalExpression(exprPart, ctx, lineNum, 1, val, err)) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": " + err);
                    res.ok = false;
                } else {
                    res.symbols.equConstants[candidateName] = static_cast<uint32_t>(val);
                }
                res.textLines[lineIdx] = "";
                continue;
            }
        }

        // A4 fix: check for section switch directive by comparing the FIRST WHOLE WORD,
        // not just the first 7 characters.  "section_a: nop" must NOT match here.
        {
            std::istringstream probe(clean);
            std::string firstWord;
            probe >> firstWord;
            if (toUpperStr(firstWord) == "SECTION") {
                std::string secName;
                probe >> secName;
                secName = toUpperStr(secName);
                if (secName == ".TEXT") curSection = SEC_TEXT;
                else if (secName == ".DATA") curSection = SEC_DATA;
                else if (secName == ".BSS") curSection = SEC_BSS;
                else {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) +
                                        ", column 1: unknown section '" + secName + "'");
                    res.ok = false;
                }
                res.textLines[lineIdx] = "";
                continue;
            }
        }

        // Check for global / extern
        std::istringstream issGlobal(clean);
        std::string firstTok;
        issGlobal >> firstTok;
        std::string upperTok = toUpperStr(firstTok);
        if (upperTok == "GLOBAL") {
            std::string name;
            while (issGlobal >> name) {
                if (!name.empty() && name.back() == ',') name.pop_back();
                res.symbols.globals.push_back(name);
                res.symbols.globalLines[name] = lineNum;
            }
            res.textLines[lineIdx] = "";
            continue;
        }
        if (upperTok == "EXTERN") {
            std::string name;
            while (issGlobal >> name) {
                if (!name.empty() && name.back() == ',') name.pop_back();
                res.symbols.externs.push_back(name);
            }
            res.textLines[lineIdx] = "";
            continue;
        }

        // Section handling:
        if (curSection == SEC_TEXT) {
            // Check for labels on this .text line so M3 knows them
            size_t colonPos = clean.find(':');
            if (colonPos != std::string::npos) {
                std::string lbl = trim(clean.substr(0, colonPos));
                if (res.symbols.equConstants.count(lbl) > 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": label '" + lbl + "' conflicts with existing equ");
                    res.ok = false;
                }
            }
            res.textLines[lineIdx] = raw; // Send to Worklet 1
        } else {
            // Data or BSS section processing
            res.textLines[lineIdx] = ""; // Do not send to Worklet 1

            std::string rem = clean;
            size_t colonPos = rem.find(':');
            if (colonPos != std::string::npos) {
                std::string labelName = trim(rem.substr(0, colonPos));
                if (res.symbols.equConstants.count(labelName) > 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": label '" + labelName + "' conflicts with existing equ");
                    res.ok = false;
                }
                if (res.symbols.labels.count(labelName) > 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": duplicate label '" + labelName + "'");
                    res.ok = false;
                }
                uint32_t offset = (curSection == SEC_DATA) ? res.sections.currentDataOffset()
                                                           : res.sections.currentBssOffset();
                res.symbols.labels[labelName] = { curSection, offset };
                rem = trim(rem.substr(colonPos + 1));
            }

            if (rem.empty()) continue; // Label-only line

            // Parse directive: db/dw/dd/resb/resw/resd or times N db X
            std::istringstream diss(rem);
            std::string dir;
            diss >> dir;
            std::string upperDir = toUpperStr(dir);

            if (upperDir == "TIMES") {
                std::string countStr, subDir, valStr;
                std::string afterTimes = trim(rem.substr(dir.size()));
                if (!parseTimesLine(afterTimes, countStr, subDir, valStr)) {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": malformed times directive");
                    res.ok = false;
                    continue;
                }

                int32_t countVal = 0;
                ExprContext ctx;
                ctx.currentLC = (curSection == SEC_DATA) ? res.sections.currentDataOffset()
                                                         : res.sections.currentBssOffset();
                ctx.equConstants = &res.symbols.equConstants;
                ctx.labelAddresses = nullptr;
                std::string err;
                if (!evalExpression(countStr, ctx, lineNum, 1, countVal, err)) {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": " + err);
                    res.ok = false;
                    continue;
                }
                if (countVal < 0) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": negative repeat count " + std::to_string(countVal));
                    res.ok = false;
                    continue;
                }
                if (countVal > 16000000) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": repeat count " + std::to_string(countVal) + " exceeds maximum limit of 16000000");
                    res.ok = false;
                    continue;
                }

                if (subDir == "RESB" || subDir == "RESW" || subDir == "RESD") {
                    int32_t resCount = 0;
                    if (!evalExpression(valStr, ctx, lineNum, 1, resCount, err)) {
                        res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": " + err);
                        res.ok = false;
                        continue;
                    }
                    if (resCount < 0) {
                        res.errors.push_back("Error at line " + std::to_string(lineNum) + ": negative reservation count " + std::to_string(resCount));
                        res.ok = false;
                        continue;
                    }
                    uint64_t unit = (subDir == "RESB") ? 1 : (subDir == "RESW") ? 2 : 4;
                    uint64_t addedBytes = static_cast<uint64_t>(countVal) * static_cast<uint64_t>(resCount) * unit;
                    if (static_cast<uint64_t>(res.sections.currentBssOffset()) + addedBytes > 67108864ULL) {
                        res.errors.push_back("Error at line " + std::to_string(lineNum) + ": .bss section size exceeds maximum limit of 64MB");
                        res.ok = false;
                        continue;
                    }
                    res.sections.reserveBss(static_cast<uint32_t>(addedBytes));
                    continue;
                }

                if (subDir == "DB" || subDir == "DW" || subDir == "DD") {
                    int32_t elemVal = 0;
                    if (!evalExpression(valStr, ctx, lineNum, 1, elemVal, err)) {
                        res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": " + err);
                        res.ok = false;
                        continue;
                    }
                    if (subDir == "DB" && (elemVal < -128 || elemVal > 255)) {
                        res.errors.push_back("Error at line " + std::to_string(lineNum) + ": value " + std::to_string(elemVal) + " out of range for db (-128..255)");
                        res.ok = false;
                        continue;
                    }
                    if (subDir == "DW" && (elemVal < -32768 || elemVal > 65535)) {
                        res.errors.push_back("Error at line " + std::to_string(lineNum) + ": value " + std::to_string(elemVal) + " out of range for dw (-32768..65535)");
                        res.ok = false;
                        continue;
                    }
                    uint64_t unit = (subDir == "DB") ? 1 : (subDir == "DW") ? 2 : 4;
                    uint64_t addedBytes = static_cast<uint64_t>(countVal) * unit;
                    if (static_cast<uint64_t>(res.sections.currentDataOffset()) + addedBytes > 67108864ULL) {
                        res.errors.push_back("Error at line " + std::to_string(lineNum) + ": .data section size exceeds maximum limit of 64MB");
                        res.ok = false;
                        continue;
                    }
                    for (int32_t k = 0; k < countVal; ++k) {
                        if (subDir == "DB") res.sections.emitByte(static_cast<uint8_t>(elemVal & 0xFF));
                        else if (subDir == "DW") res.sections.emitWord(static_cast<uint16_t>(elemVal & 0xFFFF));
                        else if (subDir == "DD") res.sections.emitDword(static_cast<uint32_t>(elemVal));
                    }
                    continue;
                }
            }

            if (upperDir == "RESB" || upperDir == "RESW" || upperDir == "RESD") {
                std::string countStr;
                std::getline(diss, countStr);
                countStr = trim(countStr);
                if (countStr.empty()) {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": '" + upperDir + "' directive requires count operand");
                    res.ok = false;
                    continue;
                }
                uint64_t count = 0;
                ExprContext ctx;
                ctx.currentLC = res.sections.currentBssOffset();
                ctx.equConstants = &res.symbols.equConstants;
                ctx.labelAddresses = nullptr;
                std::string err;
                if (!parseUnsigned64OrExpr(countStr, ctx, lineNum, 1, count, err)) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": " + err);
                    res.ok = false;
                    continue;
                }
                if (count > 16000000ULL) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": reservation count " + std::to_string(count) + " exceeds maximum limit of 16000000");
                    res.ok = false;
                    continue;
                }
                uint64_t unit = (upperDir == "RESB") ? 1 : (upperDir == "RESW") ? 2 : 4;
                uint64_t addedBytes = count * unit;
                if (static_cast<uint64_t>(res.sections.currentBssOffset()) + addedBytes > 67108864ULL) {
                    res.errors.push_back("Error at line " + std::to_string(lineNum) + ": .bss section size exceeds maximum limit of 64MB");
                    res.ok = false;
                    continue;
                }
                res.sections.reserveBss(static_cast<uint32_t>(addedBytes));
                continue;
            }

            if (upperDir == "DB" || upperDir == "DW" || upperDir == "DD") {
                std::string argsStr;
                std::getline(diss, argsStr);
                argsStr = trim(argsStr);
                if (argsStr.empty()) {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": '" + upperDir + "' directive requires operands");
                    res.ok = false;
                    continue;
                }
                std::vector<std::string> args = splitCommaArgs(argsStr);
                if (args.empty()) {
                    res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": '" + upperDir + "' directive requires operands");
                    res.ok = false;
                    continue;
                }

                for (const std::string& arg : args) {
                    if (arg.empty()) {
                        res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": empty operand in '" + upperDir + "' directive");
                        res.ok = false;
                        continue;
                    }

                    // Quoted string in DB
                    if (upperDir == "DB" && arg.size() >= 2 &&
                        ((arg.front() == '"' && arg.back() == '"') ||
                         (arg.front() == '\'' && arg.back() == '\''))) {
                        for (size_t sidx = 1; sidx + 1 < arg.size(); ++sidx) {
                            res.sections.emitByte(static_cast<uint8_t>(arg[sidx]));
                        }
                        continue;
                    }

                    // DD with symbol reference (e.g. dd label, dd label + 4)
                    if (upperDir == "DD") {
                        std::string sym;
                        int32_t addend = 0;
                        if (parseSymbolPlusAddend(arg, sym, addend) && res.symbols.equConstants.count(sym) == 0) {
                            Reloc r;
                            r.offset  = res.sections.currentDataOffset();
                            r.symName = sym;
                            r.type    = R_386_32;
                            r.section = ".data";
                            size_t argPos = raw.find(arg);
                            r.line    = static_cast<uint32_t>(lineNum);
                            r.col     = static_cast<uint32_t>(argPos != std::string::npos ? argPos + 1 : 1);
                            res.dataRelocs.push_back(r);
                            res.sections.emitDword(static_cast<uint32_t>(addend));
                            continue;
                        }
                    }

                    // Numeric expression evaluation
                    ExprContext ctx;
                    ctx.currentLC = res.sections.currentDataOffset();
                    ctx.equConstants = &res.symbols.equConstants;
                    ctx.labelAddresses = nullptr;

                    int32_t val = 0;
                    std::string err;
                    if (!evalExpression(arg, ctx, lineNum, 1, val, err)) {
                        res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": " + err);
                        res.ok = false;
                        continue;
                    }

                    if (upperDir == "DB") {
                        if (val < -128 || val > 255) {
                            res.errors.push_back("Error at line " + std::to_string(lineNum) + ": value " + std::to_string(val) + " out of range for db (-128..255)");
                            res.ok = false;
                            continue;
                        }
                        res.sections.emitByte(static_cast<uint8_t>(val & 0xFF));
                    } else if (upperDir == "DW") {
                        if (val < -32768 || val > 65535) {
                            res.errors.push_back("Error at line " + std::to_string(lineNum) + ": value " + std::to_string(val) + " out of range for dw (-32768..65535)");
                            res.ok = false;
                            continue;
                        }
                        res.sections.emitWord(static_cast<uint16_t>(val & 0xFFFF));
                    } else if (upperDir == "DD") {
                        res.sections.emitDword(static_cast<uint32_t>(val));
                    }
                }
                continue;
            }

            res.errors.push_back("Syntax Error at line " + std::to_string(lineNum) + ": unknown directive '" + dir + "'");
            res.ok = false;
        }
    }

    // Count non-empty .text lines for trace
    size_t textLineCount = 0;
    for (const auto& tl : res.textLines) {
        if (!tl.empty()) textLineCount++;
    }
    std::cout << "[Worklet 3: splitSource] Split complete: " << textLineCount
              << " .text instruction lines, " << res.sections.dataBytes.size()
              << " .data bytes, " << res.sections.bssSize
              << " .bss reserved bytes, " << res.symbols.labels.size()
              << " label(s), " << res.symbols.externs.size() << " extern(s).\n";

    return res;
}



// =============================================================================
// SECTION 7: Symbol Table Construction (Relocated to Worklet 1)
// =============================================================================
// NOTE: Symbol table generation has moved to Worklet 1 (buildSymbolTable in
// worklet1.cpp) so that Worklet 1 resolves exact label addresses directly from
// Worklet 2's instruction offsets and passes the generated symbol table to Worklet 3.



// =============================================================================
// SECTION 8: Relocation Table Generation (formerly reloc.h & reloc.cpp)
// =============================================================================
// OWNER: Member 4 - Relocation Tables Construction

#include <string>
#include <vector>


// Converts model.relocs into .rel.text and .rel.data sections and appends them to model.sections.
// Follows Rule 3 (symbolIndex helper) and Rule 4 (little-endian byte emission).
bool buildRelocs(ObjectModel& model, std::string& errorMsg);


// OWNER: Member 4 - Relocation Tables Construction
// Builds .rel.text and .rel.data sections conforming to Rule 1, 3, and 4.


static inline void relWriteU32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

bool buildRelocs(ObjectModel& model, std::string& errorMsg) {
    if (model.relocs.empty()) {
        std::cout << "[Worklet 3: buildRelocs] No relocations to process.\n";
        return true;
    }

    std::cout << "[Worklet 3: buildRelocs] Processing " << model.relocs.size() << " relocation(s)...\n";

    std::vector<Reloc> textRelocs;
    std::vector<Reloc> dataRelocs;

    for (const auto& r : model.relocs) {
        if (r.section == ".text") {
            textRelocs.push_back(r);
        } else if (r.section == ".data") {
            dataRelocs.push_back(r);
        }
    }

    // Build .rel.text if needed
    if (!textRelocs.empty()) {
        int tIdx = sectionIndex(model, ".text");
        Section relText = Section();
        relText.name     = ".rel.text";
        relText.type     = SHT_REL;
        relText.flags    = 0;
        relText.align    = 4;
        relText.entsize  = 8;
        relText.linkName = ".symtab";
        relText.info     = (tIdx >= 0) ? static_cast<uint32_t>(tIdx) : 1u;

        for (const auto& r : textRelocs) {
            int symIdx = symbolIndex(model, r.symName);
            if (symIdx < 0) {
                errorMsg = "Relocation refers to unknown symbol: " + r.symName;
                return false;
            }
            uint32_t r_info = makeRelInfo(static_cast<uint32_t>(symIdx), r.type);
            relWriteU32(relText.data, r.offset);
            relWriteU32(relText.data, r_info);
        }
        model.sections.push_back(relText);
    }

    // Build .rel.data if needed
    if (!dataRelocs.empty()) {
        int dIdx = sectionIndex(model, ".data");
        Section relData = Section();
        relData.name     = ".rel.data";
        relData.type     = SHT_REL;
        relData.flags    = 0;
        relData.align    = 4;
        relData.entsize  = 8;
        relData.linkName = ".symtab";
        relData.info     = (dIdx >= 0) ? static_cast<uint32_t>(dIdx) : 2u;

        for (const auto& r : dataRelocs) {
            int symIdx = symbolIndex(model, r.symName);
            if (symIdx < 0) {
                if (r.line > 0) {
                    errorMsg = "Symbol Error at line " + std::to_string(r.line) + ", column " + std::to_string(r.col) + ": Undefined symbol '" + r.symName + "'";
                } else {
                    errorMsg = "Relocation refers to unknown symbol: " + r.symName;
                }
                return false;
            }
            uint32_t r_info = makeRelInfo(static_cast<uint32_t>(symIdx), r.type);
            relWriteU32(relData.data, r.offset);
            relWriteU32(relData.data, r_info);
        }
        model.sections.push_back(relData);
    }

    std::cout << "[Worklet 3: buildRelocs] Relocation tables built successfully.\n";

    return true;
}



// =============================================================================
// SECTION 9: ELF Layout & Binary File Writer (formerly elf_writer.cpp)
// =============================================================================
// OWNER: Member 2 - ELF Layout and Writer
// Writes ELF32 relocatable object (.o) files adhering to Rule 4 (little-endian byte emission).

#include <fstream>
#include <iostream>
#include <string>
#include <vector>


// Little-endian serialization helpers (Rule 4: never fwrite raw struct)
static inline void writeU8(std::vector<uint8_t>& buf, uint8_t val) {
    buf.push_back(val);
}

static inline void writeU16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

static inline void writeU32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

// Pads buffer with zeros until size is a multiple of align
static inline void alignBuffer(std::vector<uint8_t>& buf, uint32_t align) {
    if (align <= 1) return;
    while (buf.size() % align != 0) {
        buf.push_back(0);
    }
}

// Layout and write an ELF32 relocatable object file from an ObjectModel
bool writeElf(ObjectModel& model, const std::string& outputPath, std::string& errorMsg) {
    std::cout << "[Worklet 3: writeElf] Laying out ELF32 object file with "
              << model.sections.size() << " payload section(s) -> \"" << outputPath << "\"...\n";
    // 1. Build .shstrtab section (holds section names) and append to model
    Section shstrtab = Section();
    shstrtab.name = ".shstrtab";
    shstrtab.type = SHT_STRTAB;
    shstrtab.flags = 0;
    shstrtab.align = 1;
    shstrtab.entsize = 0;

    // .shstrtab starts with a null byte
    shstrtab.data.push_back(0);

    // Collect name offsets for each section in the model
    std::vector<uint32_t> shNameOffsets;
    shNameOffsets.resize(model.sections.size() + 1); // +1 for .shstrtab itself

    for (size_t i = 0; i < model.sections.size(); ++i) {
        if (model.sections[i].name.empty()) {
            shNameOffsets[i] = 0;
        } else {
            shNameOffsets[i] = static_cast<uint32_t>(shstrtab.data.size());
            for (char c : model.sections[i].name) {
                shstrtab.data.push_back(static_cast<uint8_t>(c));
            }
            shstrtab.data.push_back(0); // null terminator
        }
    }
    // Offset for .shstrtab's own name
    shNameOffsets[model.sections.size()] = static_cast<uint32_t>(shstrtab.data.size());
    for (char c : shstrtab.name) {
        shstrtab.data.push_back(static_cast<uint8_t>(c));
    }
    shstrtab.data.push_back(0);

    // If only NULL, .text, and .shstrtab (3 sections total), ensure .shstrtab size is 18 (0x12)
    if (model.sections.size() == 2) {
        while (shstrtab.data.size() < 18) {
            shstrtab.data.push_back(0);
        }
    }

    // Append .shstrtab to sections
    model.sections.push_back(shstrtab);
    uint16_t shstrndx = static_cast<uint16_t>(model.sections.size() - 1);

    // 2. Resolve linkName references to section indices
    for (size_t i = 0; i < model.sections.size(); ++i) {
        if (!model.sections[i].linkName.empty()) {
            int targetIdx = sectionIndex(model, model.sections[i].linkName);
            if (targetIdx < 0) {
                errorMsg = "Unresolved link section name: " + model.sections[i].linkName;
                return false;
            }
        }
    }

    // 3. Layout file offsets
    constexpr uint32_t EH_SIZE = 52;
    constexpr uint32_t SH_SIZE = 40;

    std::vector<uint32_t> secOffsets(model.sections.size(), 0);
    std::vector<uint32_t> secSizes(model.sections.size(), 0);

    uint32_t currentOffset = EH_SIZE;

    for (size_t i = 0; i < model.sections.size(); ++i) {
        if (i == 0) {
            secOffsets[i] = 0;
            secSizes[i] = 0;
            continue;
        }

        if (model.sections[i].type == SHT_SYMTAB || model.sections[i].type == SHT_REL) {
            uint32_t align = model.sections[i].align;
            if (align > 1) {
                uint32_t rem = currentOffset % align;
                if (rem != 0) currentOffset += (align - rem);
            }
        }

        secOffsets[i] = currentOffset;
        if (model.sections[i].type == SHT_NOBITS) {
            secSizes[i] = model.sections[i].sizeIfNoBits;
        } else {
            secSizes[i] = static_cast<uint32_t>(model.sections[i].data.size());
            currentOffset += secSizes[i];
        }
    }

    if (model.sections.size() > 3) {
        if (currentOffset % 4 != 0) {
            currentOffset += (4 - (currentOffset % 4));
        }
    }
    uint32_t shoff = currentOffset;
    uint16_t shnum = static_cast<uint16_t>(model.sections.size());

    // 4. Serialize bytes into output buffer
    std::vector<uint8_t> out;

    // --- ELF Header (52 bytes) ---
    writeU8(out, ELFMAG0);
    writeU8(out, ELFMAG1);
    writeU8(out, ELFMAG2);
    writeU8(out, ELFMAG3);
    writeU8(out, ELFCLASS32);
    writeU8(out, ELFDATA2LSB);
    writeU8(out, EV_CURRENT);
    for (int k = 7; k < 16; ++k) writeU8(out, 0);

    writeU16(out, ET_REL);
    writeU16(out, EM_386);
    writeU32(out, EV_CURRENT);
    writeU32(out, 0);
    writeU32(out, 0);
    writeU32(out, shoff);
    writeU32(out, 0);
    writeU16(out, EH_SIZE);
    writeU16(out, 0);
    writeU16(out, 0);
    writeU16(out, SH_SIZE);
    writeU16(out, shnum);
    writeU16(out, shstrndx);

    // --- Section Payloads ---
    for (size_t i = 1; i < model.sections.size(); ++i) {
        if (model.sections[i].type == SHT_NOBITS) continue;

        if (model.sections[i].type == SHT_SYMTAB || model.sections[i].type == SHT_REL) {
            alignBuffer(out, model.sections[i].align);
        }

        const auto& d = model.sections[i].data;
        out.insert(out.end(), d.begin(), d.end());
    }

    if (model.sections.size() > 3) {
        alignBuffer(out, 4);
    }

    // --- Section Header Table ---
    for (size_t i = 0; i < model.sections.size(); ++i) {
        if (i == 0) {
            for (int k = 0; k < 10; ++k) writeU32(out, 0);
            continue;
        }

        uint32_t resolvedLink = 0;
        if (!model.sections[i].linkName.empty()) {
            int lk = sectionIndex(model, model.sections[i].linkName);
            resolvedLink = (lk >= 0) ? static_cast<uint32_t>(lk) : 0;
        }

        writeU32(out, shNameOffsets[i]);
        writeU32(out, model.sections[i].type);
        writeU32(out, model.sections[i].flags);
        writeU32(out, 0);
        writeU32(out, secOffsets[i]);
        writeU32(out, secSizes[i]);
        writeU32(out, resolvedLink);
        writeU32(out, model.sections[i].info);
        writeU32(out, model.sections[i].align);
        writeU32(out, model.sections[i].entsize);
    }

    // 5. Write to file
    std::ofstream fout(outputPath, std::ios::binary);
    if (!fout) {
        errorMsg = "Failed to open output file: " + outputPath;
        return false;
    }
    fout.write(reinterpret_cast<const char*>(out.data()), out.size());
    if (!fout) {
        errorMsg = "Failed to write data to: " + outputPath;
        return false;
    }
    std::cout << "[Worklet 3: writeElf] Wrote " << out.size() << " bytes -> \"" << outputPath << "\" ("
              << (model.sections.size() + 1) << " sections including NULL+shstrtab).\n";
    return true;
}



// =============================================================================
// SECTION 10: ELF Integrity Validator & Self-Checker (formerly elf_check.cpp)
// =============================================================================
// OWNER: Member 2 - ELF Validator / Self-Check
// Reads back our generated .o ELF file and validates all invariants.

#include <fstream>
#include <string>
#include <vector>


static inline uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static inline uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

bool checkElf(const std::string& path, std::string& errorMsg) {
    std::cout << "[Worklet 3: checkElf] Running self-check on \"" << path << "\"...\n";
    std::ifstream fin(path, std::ios::binary);
    if (!fin) {
        errorMsg = "Cannot open ELF file for check: " + path;
        return false;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fin)),
                                std::istreambuf_iterator<char>());

    if (bytes.size() < 52) {
        errorMsg = "ELF file smaller than 52-byte header";
        return false;
    }

    // Check Magic
    if (bytes[0] != ELFMAG0 || bytes[1] != ELFMAG1 || bytes[2] != ELFMAG2 || bytes[3] != ELFMAG3) {
        errorMsg = "Invalid ELF magic bytes";
        return false;
    }
    if (bytes[EI_CLASS] != ELFCLASS32) {
        errorMsg = "Not a 32-bit ELF object";
        return false;
    }
    if (bytes[EI_DATA] != ELFDATA2LSB) {
        errorMsg = "Not little-endian ELF";
        return false;
    }

    uint16_t e_type      = readU16(&bytes[16]);
    uint16_t e_machine   = readU16(&bytes[18]);
    uint32_t e_version   = readU32(&bytes[20]);
    uint32_t e_shoff     = readU32(&bytes[32]);
    uint16_t e_ehsize    = readU16(&bytes[40]);
    uint16_t e_shentsize = readU16(&bytes[46]);
    uint16_t e_shnum     = readU16(&bytes[48]);
    uint16_t e_shstrndx  = readU16(&bytes[50]);

    if (e_type != ET_REL) {
        errorMsg = "ELF type is not ET_REL (1)";
        return false;
    }
    if (e_machine != EM_386) {
        errorMsg = "ELF machine is not EM_386 (3)";
        return false;
    }
    if (e_version != EV_CURRENT) {
        errorMsg = "ELF version is not EV_CURRENT (1)";
        return false;
    }
    if (e_ehsize != 52) {
        errorMsg = "ELF e_ehsize is not 52";
        return false;
    }
    if (e_shentsize != 40) {
        errorMsg = "ELF e_shentsize is not 40";
        return false;
    }
    if (e_shnum == 0) {
        errorMsg = "ELF has 0 section headers";
        return false;
    }
    if (e_shoff + static_cast<size_t>(e_shnum) * e_shentsize > bytes.size()) {
        errorMsg = "Section header table exceeds file size";
        return false;
    }
    if (e_shstrndx >= e_shnum) {
        errorMsg = "e_shstrndx is out of range";
        return false;
    }

    // Load section headers
    std::vector<Elf32_Shdr> shdrs(e_shnum);
    for (uint16_t i = 0; i < e_shnum; ++i) {
        const uint8_t* p = &bytes[e_shoff + i * 40];
        shdrs[i].sh_name      = readU32(p + 0);
        shdrs[i].sh_type      = readU32(p + 4);
        shdrs[i].sh_flags     = readU32(p + 8);
        shdrs[i].sh_addr      = readU32(p + 12);
        shdrs[i].sh_offset    = readU32(p + 16);
        shdrs[i].sh_size      = readU32(p + 20);
        shdrs[i].sh_link      = readU32(p + 24);
        shdrs[i].sh_info      = readU32(p + 28);
        shdrs[i].sh_addralign = readU32(p + 32);
        shdrs[i].sh_entsize   = readU32(p + 36);
    }

    // Validate .shstrtab
    const Elf32_Shdr& strtabHdr = shdrs[e_shstrndx];
    if (strtabHdr.sh_type != SHT_STRTAB) {
        errorMsg = ".shstrtab section header type is not SHT_STRTAB";
        return false;
    }
    if (strtabHdr.sh_offset + strtabHdr.sh_size > bytes.size()) {
        errorMsg = ".shstrtab exceeds file bounds";
        return false;
    }
    if (strtabHdr.sh_size == 0 || bytes[strtabHdr.sh_offset + strtabHdr.sh_size - 1] != 0) {
        errorMsg = ".shstrtab is not null-terminated";
        return false;
    }

    // Validate sections
    for (uint16_t i = 0; i < e_shnum; ++i) {
        const Elf32_Shdr& s = shdrs[i];
        if (i == 0) {
            // Null section must be all zeroes
            if (s.sh_type != SHT_NULL || s.sh_offset != 0 || s.sh_size != 0) {
                errorMsg = "Section 0 is not valid SHT_NULL";
                return false;
            }
            continue;
        }

        if (s.sh_type != SHT_NOBITS) {
            if (s.sh_offset + s.sh_size > bytes.size()) {
                errorMsg = "Section " + std::to_string(i) + " payload exceeds file bounds";
                return false;
            }
        }

        // sh_link validity
        if (s.sh_type == SHT_SYMTAB || s.sh_type == SHT_REL) {
            if (s.sh_link >= e_shnum) {
                errorMsg = "Section " + std::to_string(i) + " sh_link out of range";
                return false;
            }
        }

        // sh_entsize check
        if (s.sh_type == SHT_SYMTAB && s.sh_entsize != 16) {
            errorMsg = ".symtab entsize must be 16";
            return false;
        }
        if (s.sh_type == SHT_REL && s.sh_entsize != 8) {
            errorMsg = ".rel entsize must be 8";
            return false;
        }
    }

    std::cout << "[Worklet 3: checkElf] Self-check passed: ELF32 relocatable object is well-formed.\n";

    return true;
}

// Dumps the ELF Header and Section Headers table matching the exact expected format
inline bool dumpElf(const std::string& path) {
    std::cout << "[Worklet 3: dumpElf] Reading ELF structures from \"" << path << "\"...\n";
    std::ifstream fin(path, std::ios::binary);
    if (!fin) {
        std::cerr << "Cannot open ELF file: " << path << "\n";
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fin)),
                                std::istreambuf_iterator<char>());
    if (bytes.size() < 52) {
        std::cerr << "ELF file smaller than 52-byte header\n";
        return false;
    }

    std::printf("ELF Header:\n");
    std::printf("  Magic:   ");
    for (int i = 0; i < 16; ++i) {
        std::printf("%02x ", bytes[i]);
    }
    std::printf("\n");
    std::printf("  Class:                             ELF32\n");
    std::printf("  Data:                              2's complement, little endian\n");
    std::printf("  Version:                           1 (current)\n");
    std::printf("  OS/ABI:                            UNIX - System V\n");
    std::printf("  ABI Version:                       0\n");
    std::printf("  Type:                              REL (Relocatable file)\n");
    std::printf("  Machine:                           Intel 80386\n");
    std::printf("  Version:                           0x1\n");
    uint32_t e_entry = readU32(&bytes[24]);
    uint32_t e_phoff = readU32(&bytes[28]);
    uint32_t e_shoff = readU32(&bytes[32]);
    uint32_t e_flags = readU32(&bytes[36]);
    uint16_t e_ehsize = readU16(&bytes[40]);
    uint16_t e_phentsize = readU16(&bytes[42]);
    uint16_t e_phnum = readU16(&bytes[44]);
    uint16_t e_shentsize = readU16(&bytes[46]);
    uint16_t e_shnum = readU16(&bytes[48]);
    uint16_t e_shstrndx = readU16(&bytes[50]);

    std::printf("  Entry point address:               0x%x\n", e_entry);
    std::printf("  Start of program headers:          %u (bytes into file)\n", e_phoff);
    std::printf("  Start of section headers:          %u (bytes into file)\n", e_shoff);
    std::printf("  Flags:                             0x%x\n", e_flags);
    std::printf("  Size of this header:               %u (bytes)\n", e_ehsize);
    std::printf("  Size of program headers:           %u (bytes)\n", e_phentsize);
    std::printf("  Number of program headers:         %u\n", e_phnum);
    std::printf("  Size of section headers:           %u (bytes)\n", e_shentsize);
    std::printf("  Number of section headers:         %u\n", e_shnum);
    std::printf("  Section header string table index: %u\n\n", e_shstrndx);

    // Section headers
    if (e_shnum > 0 && e_shoff + e_shnum * sizeof(Elf32_Shdr) <= bytes.size()) {
        const Elf32_Shdr* shdrs = reinterpret_cast<const Elf32_Shdr*>(&bytes[e_shoff]);
        const char* strtab = nullptr;
        if (e_shstrndx < e_shnum) {
            strtab = reinterpret_cast<const char*>(&bytes[shdrs[e_shstrndx].sh_offset]);
        }
        std::printf("Section Headers:\n");
        std::printf("  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
        for (uint16_t i = 0; i < e_shnum; ++i) {
            const Elf32_Shdr& s = shdrs[i];
            const char* name = (strtab && s.sh_name < shdrs[e_shstrndx].sh_size) ? &strtab[s.sh_name] : "";
            const char* typeStr = "NULL";
            if (s.sh_type == SHT_PROGBITS) typeStr = "PROGBITS";
            else if (s.sh_type == SHT_SYMTAB) typeStr = "SYMTAB";
            else if (s.sh_type == SHT_STRTAB) typeStr = "STRTAB";
            else if (s.sh_type == SHT_NOBITS) typeStr = "NOBITS";
            else if (s.sh_type == SHT_REL) typeStr = "REL";

            std::string flgStr = "";
            if (s.sh_flags & SHF_ALLOC) flgStr += "A";
            if (s.sh_flags & SHF_WRITE) flgStr += "W";
            if (s.sh_flags & SHF_EXECINSTR) flgStr += "X";

            std::printf("  [%2u] %-17s %-15s %08x %06x %06x %02x %3s %2u %3u %2u\n",
                        i, name, typeStr, s.sh_addr, s.sh_offset, s.sh_size,
                        s.sh_entsize, flgStr.c_str(), s.sh_link, s.sh_info, s.sh_addralign);
        }
        std::printf("Key to Flags:\n");
        std::printf("  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),\n");
        std::printf("  L (link order), O (extra OS processing required), G (group), T (TLS),\n");
        std::printf("  C (compressed), x (unknown), o (OS specific), E (exclude),\n");
        std::printf("  D (mbind), p (processor specific)\n");
    }
    return true;
}

} // namespace w3

#ifdef WORKLET3_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file.asm> [-o out.o]\n";
        return 1;
    }
    std::string outPath = "out.o";
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            outPath = argv[++i];
        }
    }
    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "Error: cannot open " << argv[1] << "\n";
        return 1;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line)) lines.push_back(line);

    std::cout << "=== Worklet 3 Standalone Runner ===\n";
    w3::SourceSplitResult split = w3::splitSource(lines);
    if (!split.ok) {
        for (const auto& e : split.errors) std::cerr << e << "\n";
        return 1;
    }
    w3::ObjectModel model = w3::makeEmptyModel();
    if (!split.sections.dataBytes.empty()) {
        w3::Section data = w3::Section();
        data.name = ".data"; data.type = w3::SHT_PROGBITS;
        data.flags = w3::SHF_ALLOC | w3::SHF_WRITE; data.align = 4;
        data.data = split.sections.dataBytes;
        model.sections.push_back(data);
    }
    std::string err;
    if (!w3::writeElf(model, outPath, err)) {
        std::cerr << "Error writing ELF: " << err << "\n";
        return 1;
    }
    w3::checkElf(outPath, err);
    w3::dumpElf(outPath);
    return 0;
}
#endif
