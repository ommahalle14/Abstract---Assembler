
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
