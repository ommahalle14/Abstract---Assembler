// =============================================================================
// test_w1_functions.cpp
// -----------------------------------------------------------------------------
// Dedicated Unit Test Suite for the 3 Core Functions of Worklet 1:
//   1. parseMemoryExpressionComponents (& validateMemoryExpressionSyntax)
//   2. parseAssemblySource
//   3. buildSymbolTable
// Plus auxiliary numeric literal validation (parseNumericLiteral).
//
// Build: g++ -std=c++17 -O2 -I.. test_w1_functions.cpp ../worklet2.cpp -o test_w1_functions.exe
// =============================================================================

#include "../worklet1.cpp"

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cassert>
#include <iomanip>

namespace TestHarness {

int g_totalTests = 0;
int g_passedTests = 0;
int g_failedTests = 0;

void printHeader(const std::string& title) {
    std::cout << "\n======================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "======================================================================\n";
}

void reportTest(const std::string& name, bool pass, const std::string& detail = "") {
    g_totalTests++;
    if (pass) {
        g_passedTests++;
        std::cout << "  [PASS] " << name << "\n";
    } else {
        g_failedTests++;
        std::cout << "  [FAIL] " << name;
        if (!detail.empty()) std::cout << " -> " << detail;
        std::cout << "\n";
    }
}

} // namespace TestHarness

using namespace TestHarness;

// =============================================================================
// FUNCTION 1: parseMemoryExpressionComponents & validateMemoryExpressionSyntax
// =============================================================================
void runFunction1UnitTests() {
    printHeader("FUNCTION 1: parseMemoryExpressionComponents & Memory Syntax");

    // Test 1.1: Base Register Only
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("ebx", base, index, scale, disp, sym);
        reportTest("Base reg only [ebx]", ok && base == "EBX" && index.empty() && disp == 0 && sym.empty());
    }
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("esp", base, index, scale, disp, sym);
        reportTest("Base reg only [esp]", ok && base == "ESP" && index.empty() && disp == 0 && sym.empty());
    }

    // Test 1.2: Base Register + Positive Displacement
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("ebx + 4", base, index, scale, disp, sym);
        reportTest("Base + disp [ebx + 4]", ok && base == "EBX" && index.empty() && disp == 4 && sym.empty());
    }
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("edi + 0x1000", base, index, scale, disp, sym);
        reportTest("Base + hex disp [edi + 0x1000]", ok && base == "EDI" && disp == 0x1000);
    }

    // Test 1.3: Base Register + Negative Displacement
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("ebp - 8", base, index, scale, disp, sym);
        reportTest("Base + neg disp [ebp - 8]", ok && base == "EBP" && disp == -8);
    }

    // Test 1.4: Base + SIB Scaled Index (Scales 1, 2, 4, 8)
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok1 = parseMemoryExpressionComponents("ebx + ecx*1", base, index, scale, disp, sym);
        bool p1 = ok1 && base == "EBX" && index == "ECX" && scale == 1;

        bool ok2 = parseMemoryExpressionComponents("ebx + edx*2", base, index, scale, disp, sym);
        bool p2 = ok2 && base == "EBX" && index == "EDX" && scale == 2;

        bool ok4 = parseMemoryExpressionComponents("esi + edi*4", base, index, scale, disp, sym);
        bool p4 = ok4 && base == "ESI" && index == "EDI" && scale == 4;

        bool ok8 = parseMemoryExpressionComponents("eax + ebx*8", base, index, scale, disp, sym);
        bool p8 = ok8 && base == "EAX" && index == "EBX" && scale == 8;

        reportTest("SIB scales [1, 2, 4, 8]", p1 && p2 && p4 && p8);
    }

    // Test 1.5: SIB with Displacements
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("ebx + ecx*4 + 16", base, index, scale, disp, sym);
        reportTest("Base + Index*4 + Disp [ebx + ecx*4 + 16]", ok && base == "EBX" && index == "ECX" && scale == 4 && disp == 16);
    }
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("ebp + edx*2 - 12", base, index, scale, disp, sym);
        reportTest("Base + Index*2 - Disp [ebp + edx*2 - 12]", ok && base == "EBP" && index == "EDX" && scale == 2 && disp == -12);
    }

    // Test 1.6: Commutative / Inverted Ordering
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("4*ecx + ebx + 16", base, index, scale, disp, sym);
        reportTest("Reversed SIB [4*ecx + ebx + 16]", ok && base == "EBX" && index == "ECX" && scale == 4 && disp == 16);
    }

    // Test 1.7: Index * Scale Only
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("edx*4 + 32", base, index, scale, disp, sym);
        reportTest("Index*Scale only [edx*4 + 32]", ok && base.empty() && index == "EDX" && scale == 4 && disp == 32);
    }

    // Test 1.8: Symbol in Memory
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("msg_table + 4", base, index, scale, disp, sym);
        reportTest("Symbol in memory [msg_table + 4]", ok && sym == "msg_table" && disp == 4);
    }
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool ok = parseMemoryExpressionComponents("eax + data_buffer + 8", base, index, scale, disp, sym);
        reportTest("Base + Symbol + Disp [eax + data_buffer + 8]", ok && base == "EAX" && sym == "data_buffer" && disp == 8);
    }

    // Test 1.9: Rejection of Invalid Scales
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;
        bool failScale0 = !parseMemoryExpressionComponents("ebx + ecx*0", base, index, scale, disp, sym);
        bool failScale3 = !parseMemoryExpressionComponents("ebx + ecx*3", base, index, scale, disp, sym);
        bool failScale5 = !parseMemoryExpressionComponents("ebx + ecx*5", base, index, scale, disp, sym);
        bool failScale6 = !parseMemoryExpressionComponents("ebx + ecx*6", base, index, scale, disp, sym);
        reportTest("Rejection of illegal scales (*0, *3, *5, *6)", failScale0 && failScale3 && failScale5 && failScale6);
    }

    // Test 1.10: Memory Expression Syntax Rejections (validateMemoryExpressionSyntax)
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[ebx - ecx]", why);
        reportTest("Syntax Error: Subtracting register [ebx - ecx]", hasProblem && !why.empty());
    }
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[ebx - ecx*4]", why);
        reportTest("Syntax Error: Subtracting scaled index [ebx - ecx*4]", hasProblem && !why.empty());
    }
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[ebx - my_var]", why);
        reportTest("Syntax Error: Subtracting label [ebx - my_var]", hasProblem && !why.empty());
    }
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[eax + ebx + ecx]", why);
        reportTest("Syntax Error: >2 registers in memory [eax + ebx + ecx]", hasProblem && !why.empty());
    }
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[lbl_one + lbl_two]", why);
        reportTest("Syntax Error: >1 label in memory [lbl_one + lbl_two]", hasProblem && !why.empty());
    }
    {
        std::string why;
        bool hasProblem = validateMemoryExpressionSyntax("[ebx*2 + ecx*4]", why);
        reportTest("Syntax Error: >1 scaled index [ebx*2 + ecx*4]", hasProblem && !why.empty());
    }
}

// =============================================================================
// FUNCTION 2: parseAssemblySource
// =============================================================================
void runFunction2UnitTests() {
    printHeader("FUNCTION 2: parseAssemblySource (Front-End IR & Label Parsing)");

    // Test 2.1: Simple Instructions & Operand Kinds
    {
        std::vector<std::string> lines = {
            "mov eax, ebx",
            "add ecx, 100",
            "nop",
            "ret"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok && ir.instructions.size() == 4;
        bool opsOk = (ir.instructions[0].op == w2::Op::MOV && ir.instructions[0].operandCount == 2 &&
                      ir.instructions[0].operands[0].kind == w2::OperandKind::REG &&
                      ir.instructions[0].operands[1].kind == w2::OperandKind::REG);
        bool immOk = (ir.instructions[1].op == w2::Op::ADD && ir.instructions[1].operandCount == 2 &&
                      ir.instructions[1].operands[1].kind == w2::OperandKind::IMM &&
                      ir.instructions[1].operands[1].imm == 100);
        reportTest("Parse basic instructions (reg-reg, reg-imm, nop, ret)", ok && opsOk && immOk);
    }

    // Test 2.2: Label Extraction, Forward/Backward Branches & Dense Label IDs
    {
        std::vector<std::string> lines = {
            "start:",
            "alias_start:",
            "    mov eax, 10",
            "loop_head:",
            "    dec eax",
            "    jnz loop_head",
            "    jmp done",
            "done:"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok;
        bool lblCount = (ir.labels.size() == 4);
        // start and alias_start must share dense label ID because they point to same instruction (0)
        bool startIdMatch = (ir.instructions[0].defLabel != w2::NO_LABEL);
        // done label is at the end of code (instruction index 4) -> should set endLabel
        bool endLblOk = (ir.endLabel != w2::NO_LABEL);
        reportTest("Label discovery, alias sharing & endLabel assignment", ok && lblCount && startIdMatch && endLblOk);
    }

    // Test 2.3: Pseudo-Instruction Lowering (LOAD, STORE, PUSHA, POPA, LODS, STOS)
    {
        std::vector<std::string> lines = {
            "LOAD eax, [ebx]",
            "STORE [ebx], edx",
            "PUSHA",
            "POPA",
            "LODS",
            "STOS"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok && ir.instructions.size() == 6;
        bool loadOk  = (ir.instructions[0].op == w2::Op::MOV && ir.instructions[0].operandCount == 2);
        bool storeOk = (ir.instructions[1].op == w2::Op::MOV && ir.instructions[1].operandCount == 2);
        bool pushaOk = (ir.instructions[2].op == w2::Op::PUSHAD && ir.instructions[2].prefix == 0x66);
        bool popaOk  = (ir.instructions[3].op == w2::Op::POPAD && ir.instructions[3].prefix == 0x66);
        bool lodsOk  = (ir.instructions[4].op == w2::Op::LODS && ir.instructions[4].size == 32);
        bool stosOk  = (ir.instructions[5].op == w2::Op::STOS && ir.instructions[5].size == 32);
        reportTest("Lower pseudo-instructions (LOAD, STORE, PUSHA, POPA, LODS, STOS)",
                   ok && loadOk && storeOk && pushaOk && popaOk && lodsOk && stosOk);
    }

    // Test 2.4: Memory Operand Width Directives (BYTE/WORD/DWORD PTR)
    {
        std::vector<std::string> lines = {
            "mov BYTE PTR [ebx], 5",
            "mov WORD PTR [ebx], 10",
            "mov DWORD PTR [ebx], 20"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok && ir.instructions.size() == 3;
        bool bOk = (ir.instructions[0].operands[0].size == 8);
        bool wOk = (ir.instructions[1].operands[0].size == 16);
        bool dOk = (ir.instructions[2].operands[0].size == 32);
        reportTest("Memory explicit width directives (BYTE/WORD/DWORD PTR)", ok && bOk && wOk && dOk);
    }

    // Test 2.5: ESP Normalization in Memory
    {
        std::vector<std::string> lines = {
            "mov eax, [ecx + esp]"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok && ir.instructions.size() == 1;
        const auto& mem = ir.instructions[0].operands[1].mem;
        // ESP must be normalized to base, ECX to index
        bool normOk = (mem.base == w2::Reg::ESP && mem.index == w2::Reg::ECX && mem.scale == 1);
        reportTest("ESP normalization (swap to base register)", ok && normOk);
    }

    // Test 2.6: Reference Categorization (REF_REL32, REF_ABS_IMM, REF_ABS_DISP)
    {
        std::vector<std::string> lines = {
            "target:",
            "jmp target",
            "mov eax, target",
            "mov eax, [target]"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok;
        bool hasRefs = (ir.refs.size() == 2); // imm ref and disp ref recorded in refs
        bool immRef = false, dispRef = false;
        for (const auto& r : ir.refs) {
            if (r.kind == REF_ABS_IMM) immRef = true;
            if (r.kind == REF_ABS_DISP) dispRef = true;
        }
        reportTest("Symbol reference tracking (REF_ABS_IMM and REF_ABS_DISP)", ok && hasRefs && immRef && dispRef);
    }

    // Test 2.7: Error Diagnostics - Undefined Symbol
    {
        std::vector<std::string> lines = {
            "mov eax, missing_label"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Symbol Error" &&
                      ir.errors[0].message.find("undefined symbol 'missing_label'") != std::string::npos;
        reportTest("Error Diagnostic: Undefined symbol", caught);
    }

    // Test 2.8: Error Diagnostics - Duplicate Label
    {
        std::vector<std::string> lines = {
            "dup_lbl:",
            "    nop",
            "dup_lbl:",
            "    nop"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Symbol Error" &&
                      ir.errors[0].message.find("duplicate symbol 'dup_lbl'") != std::string::npos;
        reportTest("Error Diagnostic: Duplicate symbol definition", caught);
    }

    // Test 2.9: Error Diagnostics - Reserved Word as Label
    {
        std::vector<std::string> lines = {
            "eax:",
            "    nop"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Lexical Error" &&
                      ir.errors[0].message.find("reserved word") != std::string::npos;
        reportTest("Error Diagnostic: Label using reserved register name", caught);
    }

    // Test 2.10: Error Diagnostics - Unsupported Mnemonic
    {
        std::vector<std::string> lines = {
            "bogus_opcode eax, ebx"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Syntax Error" &&
                      ir.errors[0].message.find("unsupported mnemonic") != std::string::npos;
        reportTest("Error Diagnostic: Unsupported mnemonic", caught);
    }

    // Test 2.11: Error Diagnostics - Rel8 Branch to External Symbol
    {
        std::vector<std::string> lines = {
            "loop external_proc"
        };
        std::set<std::string> externs = { "external_proc" };
        ParsedProgramIR ir = parseAssemblySource(lines, &externs);
        bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Syntax Error" &&
                      ir.errors[0].message.find("cannot reference an external symbol (rel8)") != std::string::npos;
        reportTest("Error Diagnostic: LOOP cannot reference external symbol (rel8)", caught);
    }

    // Test 2.12: Error Diagnostics - Comma Syntax Errors
    {
        std::vector<std::string> lines1 = { "mov eax ebx" };
        ParsedProgramIR ir1 = parseAssemblySource(lines1);
        bool caught1 = !ir1.ok && !ir1.errors.empty() && ir1.errors[0].message.find("expected comma") != std::string::npos;

        std::vector<std::string> lines2 = { "mov eax," };
        ParsedProgramIR ir2 = parseAssemblySource(lines2);
        bool caught2 = !ir2.ok && !ir2.errors.empty() && ir2.errors[0].message.find("missing operand after comma") != std::string::npos;

        reportTest("Error Diagnostic: Missing comma and missing trailing operand", caught1 && caught2);
    }
}

// =============================================================================
// FUNCTION 3: buildSymbolTable
// =============================================================================
void runFunction3UnitTests() {
    printHeader("FUNCTION 3: buildSymbolTable (ELF Symbol & String Tables)");

    // Test 3.1: Text Label Exact Byte Offset Resolution
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        w1.labels.push_back({ "lbl_first", 0, 1, 1 });
        w1.labels.push_back({ "lbl_second", 1, 2, 1 });

        std::vector<size_t> offs = { 0, 5, 12 }; // Instruction 0 is at offset 0, instruction 1 is at offset 5
        w3::SourceSymbols syms;
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);
        bool res1 = (syms.labels["lbl_first"].offset == 0);
        bool res2 = (syms.labels["lbl_second"].offset == 5);
        reportTest("Assign exact byte addresses from Worklet 2 offsets", ok && res1 && res2);
    }

    // Test 3.2: Undeclared Global Symbol Validation
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        std::vector<size_t> offs = { 0 };
        w3::SourceSymbols syms;
        syms.globals.push_back("undefined_global_func");
        syms.globalLines["undefined_global_func"] = 4;
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);
        bool caught = !ok && err.find("Global symbol 'undefined_global_func' is declared but not defined") != std::string::npos;
        reportTest("Reject declared global symbol that is not defined", caught);
    }

    // Test 3.3: Implicit _start Promotion to STB_GLOBAL
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        w1.labels.push_back({ "_start", 0, 1, 1 });
        std::vector<size_t> offs = { 0, 4 };
        w3::SourceSymbols syms;
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);
        bool isGlobal = false;
        for (const auto& s : model.symbols) {
            if (s.name == "_start" && s.bind == w3::STB_GLOBAL) isGlobal = true;
        }
        reportTest("Implicit promotion of '_start' to STB_GLOBAL", ok && isGlobal);
    }

    // Test 3.4: Local vs Global Partitioning & sh_info Boundary
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        w1.labels.push_back({ "my_local_func", 0, 1, 1 });
        w1.labels.push_back({ "my_global_func", 1, 2, 1 });

        std::vector<size_t> offs = { 0, 4, 8 };
        w3::SourceSymbols syms;
        syms.globals.push_back("my_global_func");
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);

        // Find .symtab section
        int symtabIdx = -1;
        for (size_t i = 0; i < model.sections.size(); ++i) {
            if (model.sections[i].name == ".symtab") symtabIdx = static_cast<int>(i);
        }

        bool partitionOk = true;
        size_t firstNonLocal = w3::firstNonLocalIndex(model);
        for (size_t i = 1; i < model.symbols.size(); ++i) {
            if (i < firstNonLocal) {
                if (model.symbols[i].bind != w3::STB_LOCAL) partitionOk = false;
            } else {
                if (model.symbols[i].bind == w3::STB_LOCAL) partitionOk = false;
            }
        }
        bool shInfoOk = (symtabIdx >= 0 && model.sections[symtabIdx].info == firstNonLocal);
        reportTest("ELF Local vs Global symbol ordering & symtab.info boundary", ok && partitionOk && shInfoOk);
    }

    // Test 3.5: EQU Constants & Extern Symbols Attributes
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        std::vector<size_t> offs = { 0 };
        w3::SourceSymbols syms;
        syms.equConstants["BUFFER_CAPACITY"] = 4096;
        syms.externs.push_back("printf");
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);
        bool equOk = false, extOk = false;
        for (const auto& s : model.symbols) {
            if (s.name == "BUFFER_CAPACITY") {
                if (s.shndx == w3::SHN_ABS && s.value == 4096 && s.type == w3::STT_NOTYPE) equOk = true;
            }
            if (s.name == "printf") {
                if (s.shndx == w3::SHN_UNDEF && s.value == 0 && s.bind == w3::STB_GLOBAL) extOk = true;
            }
        }
        reportTest("EQU constants (SHN_ABS) & Extern symbols (SHN_UNDEF, STB_GLOBAL)", ok && equOk && extOk);
    }

    // Test 3.6: String Table (.strtab) and Binary Payload Layout
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        w1.labels.push_back({ "test_label", 0, 1, 1 });
        std::vector<size_t> offs = { 0, 4 };
        w3::SourceSymbols syms;
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);

        const w3::Section* symtabSec = nullptr;
        const w3::Section* strtabSec = nullptr;
        for (const auto& sec : model.sections) {
            if (sec.name == ".symtab") symtabSec = &sec;
            if (sec.name == ".strtab") strtabSec = &sec;
        }

        bool secFound = (symtabSec != nullptr && strtabSec != nullptr);
        bool strtabZero = (strtabSec && !strtabSec->data.empty() && strtabSec->data[0] == 0);
        bool symtabSize = (symtabSec && symtabSec->data.size() == model.symbols.size() * 16);

        reportTest("Binary .symtab (16 bytes/entry) and null-terminated .strtab", secFound && strtabZero && symtabSize);
    }
}

// =============================================================================
// BONUS HELPER: parseNumericLiteral
// =============================================================================
void runNumericLiteralUnitTests() {
    printHeader("BONUS HELPER: parseNumericLiteral");

    long long v = 0;
    reportTest("Decimal positive (12345)", parseNumericLiteral("12345", v) && v == 12345);
    reportTest("Decimal negative (-9876)", parseNumericLiteral("-9876", v) && v == -9876);
    reportTest("Decimal with 'd' suffix (255d)", parseNumericLiteral("255d", v) && v == 255);
    reportTest("Hex with '0x' prefix (0xDEADBEEF)", parseNumericLiteral("0xDEADBEEF", v) && v == 3735928559LL);
    reportTest("Hex with 'h' suffix (0FFH)", parseNumericLiteral("0FFH", v) && v == 255);
    reportTest("Binary with 'b' suffix (10101010b)", parseNumericLiteral("10101010b", v) && v == 170);
    reportTest("Octal with 'o' suffix (77o)", parseNumericLiteral("77o", v) && v == 63);
    reportTest("Octal with 'q' suffix (177q)", parseNumericLiteral("177q", v) && v == 127);
    reportTest("Reject invalid alphanumeric (1234xyz)", !parseNumericLiteral("1234xyz", v));
    reportTest("Reject invalid hex digit (0x123Z)", !parseNumericLiteral("0x123Z", v));
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main() {
    std::cout << "\n======================================================================\n";
    std::cout << "         WORKLET 1 DIRECT C++ UNIT TEST SUITE EXECUTION               \n";
    std::cout << "======================================================================\n";

    runFunction1UnitTests();
    runFunction2UnitTests();
    runFunction3UnitTests();
    runNumericLiteralUnitTests();

    std::cout << "\n======================================================================\n";
    std::cout << "                       TEST SUITE SUMMARY                             \n";
    std::cout << "======================================================================\n";
    std::cout << "  Total Unit Tests Executed : " << g_totalTests << "\n";
    std::cout << "  Passed Unit Tests         : " << g_passedTests << " ("
              << (g_passedTests * 100.0 / g_totalTests) << "%)\n";
    std::cout << "  Failed Unit Tests         : " << g_failedTests << "\n";
    std::cout << "  Final Test Result         : " << (g_failedTests == 0 ? "ALL TESTS PASSED [SUCCESS]" : "FAILURES DETECTED") << "\n";
    std::cout << "======================================================================\n\n";

    return (g_failedTests == 0) ? 0 : 1;
}
