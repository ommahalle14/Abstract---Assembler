// =============================================================================
// test_parseMemoryExpressionComponents.cpp
// -----------------------------------------------------------------------------
// Unit Test Suite for Worklet 1 Major Function:
//   - parseMemoryExpressionComponents()
//   - validateMemoryExpressionSyntax()
//
// Testing Type: White Box Unit Testing (Statement, Branch & Path Coverage)
// Build: g++ -std=c++17 -O2 -I.. test_parseMemoryExpressionComponents.cpp ../worklet2.cpp -o test_parseMemoryExpressionComponents.exe
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
    std::cout << "   UNIT TESTING: parseMemoryExpressionComponents() (Worklet 1)       \n";
    std::cout << "======================================================================\n";

    printSection("1. Base Register Alone [base]");
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;

        bool okEbx = parseMemoryExpressionComponents("ebx", base, index, scale, disp, sym);
        assertTest("Base register EBX: [ebx]", okEbx && base == "EBX" && index.empty() && disp == 0 && sym.empty());

        bool okEsp = parseMemoryExpressionComponents("esp", base, index, scale, disp, sym);
        assertTest("Base register ESP: [esp]", okEsp && base == "ESP" && index.empty() && disp == 0 && sym.empty());

        bool okEbp = parseMemoryExpressionComponents("ebp", base, index, scale, disp, sym);
        assertTest("Base register EBP: [ebp]", okEbp && base == "EBP" && index.empty() && disp == 0 && sym.empty());
    }

    printSection("2. Base Register with Displacements [base + disp]");
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;

        bool okPos = parseMemoryExpressionComponents("ebx + 4", base, index, scale, disp, sym);
        assertTest("Positive decimal disp: [ebx + 4]", okPos && base == "EBX" && disp == 4);

        bool okHex = parseMemoryExpressionComponents("edi + 0x1000", base, index, scale, disp, sym);
        assertTest("Hexadecimal disp: [edi + 0x1000]", okHex && base == "EDI" && disp == 0x1000);

        bool okNeg = parseMemoryExpressionComponents("ebp - 8", base, index, scale, disp, sym);
        assertTest("Negative decimal disp: [ebp - 8]", okNeg && base == "EBP" && disp == -8);
    }

    printSection("3. SIB Scaled Index [base + index*scale]");
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;

        bool ok1 = parseMemoryExpressionComponents("ebx + ecx*1", base, index, scale, disp, sym);
        assertTest("SIB scale *1: [ebx + ecx*1]", ok1 && base == "EBX" && index == "ECX" && scale == 1);

        bool ok2 = parseMemoryExpressionComponents("ebx + edx*2", base, index, scale, disp, sym);
        assertTest("SIB scale *2: [ebx + edx*2]", ok2 && base == "EBX" && index == "EDX" && scale == 2);

        bool ok4 = parseMemoryExpressionComponents("esi + edi*4", base, index, scale, disp, sym);
        assertTest("SIB scale *4: [esi + edi*4]", ok4 && base == "ESI" && index == "EDI" && scale == 4);

        bool ok8 = parseMemoryExpressionComponents("eax + ebx*8", base, index, scale, disp, sym);
        assertTest("SIB scale *8: [eax + ebx*8]", ok8 && base == "EAX" && index == "EBX" && scale == 8);
    }

    printSection("4. Full Addressing Forms & Commutative Ordering");
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;

        bool okFull = parseMemoryExpressionComponents("ebx + ecx*4 + 16", base, index, scale, disp, sym);
        assertTest("Full SIB: [ebx + ecx*4 + 16]", okFull && base == "EBX" && index == "ECX" && scale == 4 && disp == 16);

        bool okFullNeg = parseMemoryExpressionComponents("ebp + edx*2 - 12", base, index, scale, disp, sym);
        assertTest("Full SIB with neg disp: [ebp + edx*2 - 12]", okFullNeg && base == "EBP" && index == "EDX" && scale == 2 && disp == -12);

        bool okRev = parseMemoryExpressionComponents("4*ecx + ebx + 16", base, index, scale, disp, sym);
        assertTest("Inverted/Commutative order: [4*ecx + ebx + 16]", okRev && base == "EBX" && index == "ECX" && scale == 4 && disp == 16);

        bool okIdxOnly = parseMemoryExpressionComponents("edx*4 + 32", base, index, scale, disp, sym);
        assertTest("Index*Scale only: [edx*4 + 32]", okIdxOnly && base.empty() && index == "EDX" && scale == 4 && disp == 32);
    }

    printSection("5. Symbolic Label Address Offsets");
    {
        std::string base, index, sym;
        int scale = 0;
        long long disp = 0;

        bool okSym = parseMemoryExpressionComponents("msg_table + 4", base, index, scale, disp, sym);
        assertTest("Symbol + Disp: [msg_table + 4]", okSym && sym == "msg_table" && disp == 4);

        bool okBaseSym = parseMemoryExpressionComponents("eax + data_buffer + 8", base, index, scale, disp, sym);
        assertTest("Base + Symbol + Disp: [eax + data_buffer + 8]", okBaseSym && base == "EAX" && sym == "data_buffer" && disp == 8);
    }

    printSection("6. Negative Testing & Boundary Value Syntax Rejections");
    {
        std::string base, index, sym, why;
        int scale = 0;
        long long disp = 0;

        assertTest("Reject illegal scale *0", !parseMemoryExpressionComponents("ebx + ecx*0", base, index, scale, disp, sym));
        assertTest("Reject illegal scale *3", !parseMemoryExpressionComponents("ebx + ecx*3", base, index, scale, disp, sym));
        assertTest("Reject illegal scale *5", !parseMemoryExpressionComponents("ebx + ecx*5", base, index, scale, disp, sym));
        assertTest("Reject illegal scale *6", !parseMemoryExpressionComponents("ebx + ecx*6", base, index, scale, disp, sym));

        assertTest("Syntax Error: Subtracting register [ebx - ecx]", validateMemoryExpressionSyntax("[ebx - ecx]", why) && !why.empty());
        assertTest("Syntax Error: Subtracting scaled index [ebx - ecx*4]", validateMemoryExpressionSyntax("[ebx - ecx*4]", why) && !why.empty());
        assertTest("Syntax Error: Subtracting label [ebx - my_var]", validateMemoryExpressionSyntax("[ebx - my_var]", why) && !why.empty());
        assertTest("Syntax Error: >2 registers in memory [eax + ebx + ecx]", validateMemoryExpressionSyntax("[eax + ebx + ecx]", why) && !why.empty());
        assertTest("Syntax Error: >1 label in memory [lbl_one + lbl_two]", validateMemoryExpressionSyntax("[lbl_one + lbl_two]", why) && !why.empty());
        assertTest("Syntax Error: >1 scaled index [ebx*2 + ecx*4]", validateMemoryExpressionSyntax("[ebx*2 + ecx*4]", why) && !why.empty());
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  parseMemoryExpressionComponents() RESULTS: " << g_passed << "/" << g_total
              << " (" << (g_passed * 100.0 / g_total) << "%)\n";
    std::cout << "======================================================================\n\n";

    return (g_failed == 0) ? 0 : 1;
}

