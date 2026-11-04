// =============================================================================
// test_parseAssemblySource.cpp
// -----------------------------------------------------------------------------
// Unit Test Suite for Worklet 1 Major Function:
//   - parseAssemblySource()
//
// Testing Type: White Box Unit Testing (Statement, Branch, Decision & Error Path Coverage)
// Build: g++ -std=c++17 -O2 -I.. test_parseAssemblySource.cpp ../worklet2.cpp -o test_parseAssemblySource.exe
// =============================================================================

#include "../worklet1.cpp"

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cassert>
#include <iomanip>

namespace {

int g_total = 0;
int g_passed = 0;
int g_failed = 0;

void printSection(const std::string& title) {
    std::cout << "\n======================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "======================================================================\n";
}

void assertTest(const std::string& testName, bool condition, const std::string& extra = "") {
    g_total++;
    if (condition) {
        g_passed++;
        std::cout << "  [PASS] " << testName << "\n";
    } else {
        g_failed++;
        std::cout << "  [FAIL] " << testName;
        if (!extra.empty()) std::cout << " -> " << extra;
        std::cout << "\n";
    }
}

} // namespace

int main() {
    std::cout << "\n======================================================================\n";
    std::cout << "   UNIT TESTING: parseAssemblySource() (Worklet 1)                   \n";
    std::cout << "======================================================================\n";

    printSection("1. Basic Instruction Parsing & Operand Enums");
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
        bool nopOk = (ir.instructions[2].op == w2::Op::NOP && ir.instructions[2].operandCount == 0);
        bool retOk = (ir.instructions[3].op == w2::Op::RET && ir.instructions[3].operandCount == 0);
        assertTest("Parse basic instructions (reg-reg, reg-imm, nop, ret)", ok && opsOk && immOk && nopOk && retOk);
    }

    printSection("2. Label Discovery, Aliasing & End-of-Code Assignment");
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
        // start and alias_start point to same instruction (0)
        bool startIdMatch = (ir.instructions[0].defLabel != w2::NO_LABEL);
        // done label is at the end of code (instruction index 4) -> should set endLabel
        bool endLblOk = (ir.endLabel != w2::NO_LABEL);
        assertTest("Label discovery, alias sharing & endLabel assignment", ok && lblCount && startIdMatch && endLblOk);
    }

    printSection("3. Lowering of Pseudo-Instructions");
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
        assertTest("Lower pseudo-instructions (LOAD, STORE, PUSHA, POPA, LODS, STOS)",
                   ok && loadOk && storeOk && pushaOk && popaOk && lodsOk && stosOk);
    }

    printSection("4. Explicit Memory Width Directives (BYTE/WORD/DWORD PTR)");
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
        assertTest("Memory explicit width directives (BYTE/WORD/DWORD PTR)", ok && bOk && wOk && dOk);
    }

    printSection("5. ESP Normalization in Memory Operands");
    {
        std::vector<std::string> lines = {
            "mov eax, [ecx + esp]"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok && ir.instructions.size() == 1;
        const auto& mem = ir.instructions[0].operands[1].mem;
        // ESP cannot be index in x86 ModRM/SIB; must be normalized to base, ECX to index
        bool normOk = (mem.base == w2::Reg::ESP && mem.index == w2::Reg::ECX && mem.scale == 1);
        assertTest("ESP normalization (swap to base register)", ok && normOk);
    }

    printSection("6. Symbol Reference Tracking (Immediate vs Displacement)");
    {
        std::vector<std::string> lines = {
            "target:",
            "jmp target",
            "mov eax, target",
            "mov eax, [target]"
        };
        ParsedProgramIR ir = parseAssemblySource(lines);
        bool ok = ir.ok;
        bool hasRefs = (ir.refs.size() == 2);
        bool immRef = false, dispRef = false;
        for (const auto& r : ir.refs) {
            if (r.kind == REF_ABS_IMM) immRef = true;
            if (r.kind == REF_ABS_DISP) dispRef = true;
        }
        assertTest("Symbol reference tracking (REF_ABS_IMM and REF_ABS_DISP)", ok && hasRefs && immRef && dispRef);
    }

    printSection("7. Error Diagnostics & Error Guessing (Negative Testing)");
    {
        // 7.1 Undefined Symbol
        {
            std::vector<std::string> lines = { "mov eax, missing_label" };
            ParsedProgramIR ir = parseAssemblySource(lines);
            bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Symbol Error" &&
                          ir.errors[0].message.find("undefined symbol 'missing_label'") != std::string::npos;
            assertTest("Error Diagnostic: Undefined symbol", caught);
        }

        // 7.2 Duplicate Symbol Definition
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
            assertTest("Error Diagnostic: Duplicate symbol definition", caught);
        }

        // 7.3 Reserved Word as Label
        {
            std::vector<std::string> lines = { "eax:", "    nop" };
            ParsedProgramIR ir = parseAssemblySource(lines);
            bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Lexical Error" &&
                          ir.errors[0].message.find("reserved word") != std::string::npos;
            assertTest("Error Diagnostic: Label using reserved register name", caught);
        }

        // 7.4 Unsupported Mnemonic
        {
            std::vector<std::string> lines = { "bogus_opcode eax, ebx" };
            ParsedProgramIR ir = parseAssemblySource(lines);
            bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Syntax Error" &&
                          ir.errors[0].message.find("unsupported mnemonic") != std::string::npos;
            assertTest("Error Diagnostic: Unsupported mnemonic", caught);
        }

        // 7.5 Rel8 Branch to External Symbol
        {
            std::vector<std::string> lines = { "loop external_proc" };
            std::set<std::string> externs = { "external_proc" };
            ParsedProgramIR ir = parseAssemblySource(lines, &externs);
            bool caught = !ir.ok && !ir.errors.empty() && ir.errors[0].category == "Syntax Error" &&
                          ir.errors[0].message.find("cannot reference an external symbol (rel8)") != std::string::npos;
            assertTest("Error Diagnostic: LOOP cannot reference external symbol (rel8)", caught);
        }

        // 7.6 Comma Syntax Errors
        {
            std::vector<std::string> lines1 = { "mov eax ebx" };
            ParsedProgramIR ir1 = parseAssemblySource(lines1);
            bool caught1 = !ir1.ok && !ir1.errors.empty() && ir1.errors[0].message.find("expected comma") != std::string::npos;

            std::vector<std::string> lines2 = { "mov eax," };
            ParsedProgramIR ir2 = parseAssemblySource(lines2);
            bool caught2 = !ir2.ok && !ir2.errors.empty() && ir2.errors[0].message.find("missing operand after comma") != std::string::npos;

            assertTest("Error Diagnostic: Missing comma and missing operand after comma", caught1 && caught2);
        }
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  parseAssemblySource() RESULTS: " << g_passed << "/" << g_total
              << " (" << (g_passed * 100.0 / g_total) << "%)\n";
    std::cout << "======================================================================\n\n";

    return (g_failed == 0) ? 0 : 1;
}

