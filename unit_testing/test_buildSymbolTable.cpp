// =============================================================================
// test_buildSymbolTable.cpp
// -----------------------------------------------------------------------------
// Unit Test Suite for Worklet 1 Major Function:
//   - buildSymbolTable()
//
// Testing Type: White Box Unit Testing (Boundary Value, Structural & Compliance Coverage)
// Build: g++ -std=c++17 -O2 -I.. test_buildSymbolTable.cpp ../worklet2.cpp -o test_buildSymbolTable.exe
// =============================================================================

#include "../worklet1.cpp"

#include <iostream>
#include <string>
#include <vector>
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
    std::cout << "   UNIT TESTING: buildSymbolTable() (Worklet 1)                      \n";
    std::cout << "======================================================================\n";

    printSection("1. Exact Instruction Offset Resolution");
    {
        w3::ObjectModel model = w3::makeEmptyModel();
        ParsedProgramIR w1;
        w1.labels.push_back({ "lbl_first", 0, 1, 1 });
        w1.labels.push_back({ "lbl_second", 1, 2, 1 });

        // Instruction 0 starts at byte offset 0; Instruction 1 starts at byte offset 5
        std::vector<size_t> offs = { 0, 5, 12 };
        w3::SourceSymbols syms;
        std::string err;

        bool ok = buildSymbolTable(model, w1, offs, syms, err);
        bool res1 = (syms.labels["lbl_first"].offset == 0);
        bool res2 = (syms.labels["lbl_second"].offset == 5);
        assertTest("Assign exact byte addresses from Worklet 2 offsets", ok && res1 && res2);
    }

    printSection("2. Undeclared Global Symbol Validation");
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
        assertTest("Reject declared global symbol that is not defined", caught);
    }

    printSection("3. Implicit '_start' Promotion to STB_GLOBAL");
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
        assertTest("Implicit promotion of '_start' to STB_GLOBAL", ok && isGlobal);
    }

    printSection("4. ELF Symbol Partitioning (Local vs Global) & sh_info Boundary");
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

        int symtabIdx = -1;
        for (size_t i = 0; i < model.sections.size(); ++i) {
            if (model.sections[i].name == ".symtab") symtabIdx = static_cast<int>(i);
        }

        // ELF standard rule: Local symbols must precede Global symbols
        bool partitionOk = true;
        size_t firstNonLocal = w3::firstNonLocalIndex(model);
        for (size_t i = 1; i < model.symbols.size(); ++i) {
            if (i < firstNonLocal) {
                if (model.symbols[i].bind != w3::STB_LOCAL) partitionOk = false;
            } else {
                if (model.symbols[i].bind == w3::STB_LOCAL) partitionOk = false;
            }
        }
        // symtab.info must point to firstNonLocalIndex
        bool shInfoOk = (symtabIdx >= 0 && model.sections[symtabIdx].info == firstNonLocal);
        assertTest("ELF Local vs Global symbol ordering & symtab.info boundary", ok && partitionOk && shInfoOk);
    }

    printSection("5. EQU Constants & Extern Symbols Attributes");
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
        assertTest("EQU constants (SHN_ABS) & Extern symbols (SHN_UNDEF, STB_GLOBAL)", ok && equOk && extOk);
    }

    printSection("6. Binary .symtab (16 bytes/entry) and .strtab Null-Termination");
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

        assertTest("Binary .symtab (16 bytes/entry) and null-terminated .strtab", ok && secFound && strtabZero && symtabSize);
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  buildSymbolTable() RESULTS: " << g_passed << "/" << g_total
              << " (" << (g_passed * 100.0 / g_total) << "%)\n";
    std::cout << "======================================================================\n\n";

    return (g_failed == 0) ? 0 : 1;
}
