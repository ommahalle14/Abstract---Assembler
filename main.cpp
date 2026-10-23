// main.cpp -- integrated driver:  Worklet 1  ->  Worklet 2  ->  Worklet 3
//
//   asm source
//     |  Worklet 3 (front half): splitSource()  .text lines | .data/.bss bytes | labels, global, extern, equ
//     v
//   .text lines --> Worklet 1 (tokenize, parse directly into w2::Instruction enum IR)
//     |
//     v
//   Worklet 2: encode()  --> machine code (.bin bytes) + exact byte offsets
//     |
//     v
//   Worklet 3 (back half): .text = the bytes   +   symbols from Worklet 2 offsets   +   relocations
//                          --> ELF32 relocatable (.o)
//
//   Build: g++ -std=c++17 -O2 -Wall -Wextra -o assembler main.cpp worklet2.cpp
//          (worklet1.cpp and worklet3.cpp are #included, do not compile them separately)
//   Run  : ./assembler prog.asm [-o out.o | out.bin] [--format elf|bin] [--base 0x...] [--bin flat.bin] [--listing]

#include "worklet3.cpp"
#include "worklet1.cpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <iostream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Byte emission helpers
// ---------------------------------------------------------------------------
inline uint32_t rd32(const std::vector<uint8_t>& b, size_t p) {
    return  static_cast<uint32_t>(b[p]) | (static_cast<uint32_t>(b[p + 1]) << 8) |
           (static_cast<uint32_t>(b[p + 2]) << 16) | (static_cast<uint32_t>(b[p + 3]) << 24);
}
inline void wr32(std::vector<uint8_t>& b, size_t p, uint32_t v) {
    b[p] = static_cast<uint8_t>(v);  b[p + 1] = static_cast<uint8_t>(v >> 8);
    b[p + 2] = static_cast<uint8_t>(v >> 16);  b[p + 3] = static_cast<uint8_t>(v >> 24);
}

// Where, inside ONE instruction, does the 32-bit label field live, and what does it hold?
bool probeAbsField(const w2::Instruction& inst, int opIdx, size_t& fieldPos, uint32_t& disp) {
    w2::Instruction p = inst;
    p.defLabel = w2::NO_LABEL;
    if (p.operands[opIdx].kind == w2::OperandKind::MEMLABEL) p.operands[opIdx].label = 0;
    else                                                     p.operands[opIdx].mem.label = 0;
    const w2::Instruction nop = w2::inst0(w2::Op::NOP);

    // A: [inst, nop]  label = end of code (offset L+1), base 0       B: same, base 0x01010101
    const w2::Instruction two[2] = { p, nop };
    std::vector<uint8_t> a, b;
    if (w2::encode(w2::makeProgram(two, 2, 1, 0, 0), a).status != w2::Status::OK) return false;
    if (w2::encode(w2::makeProgram(two, 2, 1, 0x01010101u, 0), b).status != w2::Status::OK) return false;
    const size_t L = a.size() - 1;
    size_t f = 0;
    while (f < L && a[f] == b[f]) ++f;
    if (f + 4 > L) return false;
    fieldPos = f;
    disp = rd32(a, f) - static_cast<uint32_t>(L + 1);
    return true;
}

// Applies symbol fixups and collects relocations
bool applyRefs(const Worklet1Output& w1, const std::vector<w2::Instruction>& code, const std::vector<size_t>& offs,
               std::vector<uint8_t>& bytes, bool elfMode, uint32_t base,
               std::vector<w3::Reloc>& relocs, std::string& error) {
    std::unordered_map<std::string, uint32_t> labelOffs;
    for (size_t i = 0; i < w1.labels.size(); ++i) {
        labelOffs[w1.labels[i].name] = static_cast<uint32_t>(offs[w1.labels[i].instrIndex]);
    }

    for (size_t k = 0; k < w1.refs.size(); ++k) {
        const SymRef& r = w1.refs[k];
        const size_t start = offs[r.instr], end = offs[r.instr + 1];
        const int line = w1.instructionLines[r.instr];

        if (!elfMode && r.external) {
            error = "line " + std::to_string(line) + ": '" + r.sym +
                    "' is an extern or a .data/.bss symbol; use ELF output (--format elf), a flat binary cannot hold relocations";
            return false;
        }

        size_t field = 0;  uint32_t addend = 0;  uint8_t type = w3::R_386_32;
        switch (r.kind) {
            case REF_REL32:
                field = end - 4;  addend = 0xFFFFFFFCu;  type = w3::R_386_PC32;
                break;
            case REF_ABS_IMM:
                field = end - 4;  addend = 0;
                if (!elfMode) {
                    uint32_t targetAddr = base + (labelOffs.count(r.sym) ? labelOffs[r.sym] : 0u);
                    wr32(bytes, field, targetAddr);
                    continue;
                }
                break;
            case REF_ABS_DISP: {
                if (!elfMode) continue;
                size_t f;  uint32_t d;
                if (!probeAbsField(code[r.instr], r.opIdx, f, d)) {
                    error = "line " + std::to_string(line) + ": internal error, cannot locate the address field of '" + r.sym + "'";
                    return false;
                }
                field = start + f;  addend = d;
                break;
            }
        }

        wr32(bytes, field, addend);
        w3::Reloc rel;
        rel.offset = static_cast<uint32_t>(field);  rel.symName = r.sym;  rel.type = type;
        rel.section = ".text";  rel.line = static_cast<uint32_t>(line);  rel.col = 0;
        relocs.push_back(rel);
    }
    std::sort(relocs.begin(), relocs.end(), [](const w3::Reloc& a, const w3::Reloc& b) { return a.offset < b.offset; });
    return true;
}

struct Options {
    bool        flat    = false;       // --format bin
    bool        listing = false;
    bool        w1Only  = false;       // --w1 / --w1-only: execute Worklet 1 only and display IR
    bool        w2Only  = false;       // --w2 / --w2-only: execute Worklet 1+2 only and emit .bin
    bool        w3Only  = false;       // --w3 / --w3-only: execute and dump Worklet 3 ELF details
    bool        dumpElf = false;       // --dump-elf: dump ELF Header & Section Headers table
    uint32_t    base    = 0;           // flat binary: address the code is assumed to be loaded at
    std::string out     = "";
    std::string alsoBin = "";          // --bin: also write the raw .text bytes
};

void printListing(const Worklet1Output& w1, const std::vector<size_t>& offs,
                  const std::vector<uint8_t>& bytes) {
    std::printf("\n======================================================================\n");
    std::printf("WORKLET 2 OUTPUT: Machine Code Disassembly Listing (.bin)\n");
    std::printf("======================================================================\n");
    for (size_t i = 0; i < w1.instructions.size(); ++i) {
        std::string hex;
        char buf[4];
        for (size_t k = offs[i]; k < offs[i + 1]; ++k) {
            std::snprintf(buf, sizeof buf, "%02X ", bytes[k]);
            hex += buf;
        }
        std::printf("%08lX  %-21s %s\n", static_cast<unsigned long>(offs[i]), hex.c_str(), w1.instructionText[i].c_str());
    }
    std::printf("Total Emitted Bytes: %lu\n", static_cast<unsigned long>(bytes.size()));
    std::printf("======================================================================\n\n");
}

// Worklet 1 parsed enum instructions -> Worklet 2 encode() -> patched bytes + relocations
bool encodeProgram(const Worklet1Output& w1, const Options& opt,
                   std::vector<uint8_t>& bytes, std::vector<size_t>& offs,
                   std::vector<w3::Reloc>& relocs, std::string& error) {
    std::vector<w2::Instruction> code = w1.instructions;
    code.push_back(w2::inst0(w2::Op::NOP, w1.endLabel));
    const uint32_t labelCount = w1.labelCount;
    const w2::Program prog = w2::makeProgram(&code[0], code.size(), labelCount,
                                             opt.flat ? opt.base : 0u, w1.extId);
    w2::Result r = w2::encode(prog, bytes, &offs);
    if (r.status != w2::Status::OK) {
        error = std::string("encoding failed: ") + w2::statusText(r.status);
        if (r.instrIndex < w1.instructionLines.size())
            error += " (line " + std::to_string(w1.instructionLines[r.instrIndex]) + ")";
        return false;
    }
    bytes.pop_back();
    offs.pop_back();
    code.pop_back();

    return applyRefs(w1, code, offs, bytes, !opt.flat, opt.base, relocs, error);
}

// Substitute `equ` constants into the .text lines
void substituteEqu(std::vector<std::string>& lines, const std::unordered_map<std::string, uint32_t>& equ) {
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string& line = lines[i];
        for (std::unordered_map<std::string, uint32_t>::const_iterator kv = equ.begin(); kv != equ.end(); ++kv) {
            size_t pos = 0;
            while ((pos = line.find(kv->first, pos)) != std::string::npos) {
                const bool leftOk  = (pos == 0 || !std::isalnum(static_cast<unsigned char>(line[pos - 1])));
                const size_t after = pos + kv->first.size();
                const bool rightOk = (after >= line.size() || !std::isalnum(static_cast<unsigned char>(line[after])));
                if (leftOk && rightOk) {
                    const std::string repl = std::to_string(kv->second);
                    line.replace(pos, kv->first.size(), repl);
                    pos += repl.size();
                } else {
                    pos += kv->first.size();
                }
            }
        }
    }
}

// The whole pipeline
int assemble(const std::vector<std::string>& rawLines, const Options& opt) {
    std::cout << "\n>>> Phase 1: Source Splitting (Worklet 3: splitSource) <<<\n";
    // ---- Worklet 3, front half: sections + directives --------------------------------------
    w3::SourceSplitResult split = w3::splitSource(rawLines);
    if (!split.ok) {
        for (size_t i = 0; i < split.errors.size(); ++i) std::cerr << split.errors[i] << "\n";
        return 1;
    }

    std::vector<std::string> externalNames = split.symbols.externs;
    for (std::unordered_map<std::string, w3::SourceSymbols::LabelInfo>::const_iterator kv = split.symbols.labels.begin();
         kv != split.symbols.labels.end(); ++kv)
        externalNames.push_back(kv->first);
    substituteEqu(split.textLines, split.symbols.equConstants);

    std::cout << "\n>>> Phase 2: Assembly Parsing & IR Generation (Worklet 1: parseAssemblySource) <<<\n";
    // ---- Worklet 1: tokenize and parse directly to enum instructions ------------------------
    std::set<std::string> externSet(externalNames.begin(), externalNames.end());
    Worklet1Output w1 = parseAssemblySource(split.textLines, &externSet);
    if (!w1.ok) {
        for (size_t i = 0; i < w1.errors.size(); ++i)
            std::cerr << w1.errors[i].category << " at line " << w1.errors[i].line << ", column "
                      << w1.errors[i].column << ": " << w1.errors[i].message << "\n";
        std::cerr << "\n" << w1.errors.size() << " error(s) found. Pipeline stopped.\n";
        return 1;
    }

    // Worklet 1 standalone request: show output and exit
    if (opt.w1Only) {
        printParsedProgramIR(w1);
        std::cout << "[Assembler] Worklet 1 completed successfully (--w1 mode).\n";
        return 0;
    }

    // ---- Worklet 2: encode -----------------------------------------------------------------
    std::cout << "\n>>> Phase 3: Machine Code Encoding (Worklet 2: encode) <<<\n";
    std::vector<uint8_t> bytes;  std::vector<size_t> offs;  std::vector<w3::Reloc> relocs;  std::string err;
    if (!encodeProgram(w1, opt, bytes, offs, relocs, err)) { std::cerr << err << "\n"; return 1; }
    if (opt.listing || opt.w2Only) printListing(w1, offs, bytes);
    if (!opt.alsoBin.empty() && !w2::writeBinFile(opt.alsoBin.c_str(), bytes)) {
        std::cerr << "could not write " << opt.alsoBin << "\n";  return 1;
    }

    if (opt.w2Only) {
        std::string binPath = opt.out.empty() ? "out.bin" : opt.out;
        if (!w2::writeBinFile(binPath.c_str(), bytes)) { std::cerr << "could not write " << binPath << "\n"; return 1; }
        std::cout << "[Assembler] Worklet 2 completed successfully: wrote " << bytes.size() << " bytes to " << binPath << " (--w2 mode).\n";
        return 0;
    }

    if (opt.flat) {
        if (!w2::writeBinFile(opt.out.c_str(), bytes)) { std::cerr << "could not write " << opt.out << "\n"; return 1; }
        std::cout << "wrote " << bytes.size() << " bytes to " << opt.out << " (flat binary)\n";
        return 0;
    }

    // Prepare ObjectModel sections
    w3::ObjectModel model = w3::makeEmptyModel();
    model.sections[1].data = bytes; // Worklet 2's .bin

    if (!split.sections.dataBytes.empty()) {
        w3::Section data = w3::Section();
        data.name = ".data"; data.type = w3::SHT_PROGBITS;
        data.flags = w3::SHF_ALLOC | w3::SHF_WRITE; data.align = 4;
        data.data = split.sections.dataBytes;
        model.sections.push_back(data);
    }
    if (split.sections.bssSize > 0) {
        w3::Section bss = w3::Section();
        bss.name = ".bss"; bss.type = w3::SHT_NOBITS;
        bss.flags = w3::SHF_ALLOC | w3::SHF_WRITE; bss.align = 4;
        bss.sizeIfNoBits = split.sections.bssSize;
        model.sections.push_back(bss);
    }

    model.relocs = relocs;
    model.relocs.insert(model.relocs.end(), split.dataRelocs.begin(), split.dataRelocs.end());

    // ---- Phase 4: Symbol Table Generation (Worklet 1) ---------------------------------------
    // Worklet 1 takes instruction addresses from Worklet 2 (offs), resolves labels,
    // builds the symbol table and string table, and passes it directly to Worklet 3!
    std::cout << "\n>>> Phase 4: Symbol Table Generation (Worklet 1: buildSymbolTable) <<<\n";
    if (!buildSymbolTable(model, w1, offs, split.symbols, err)) {
        std::cerr << "Symbol Table Error: " << err << "\n";
        return 1;
    }

    // ---- Phase 5: ELF Relocation & Object Generation (Worklet 3) ---------------------------
    std::cout << "\n>>> Phase 5: ELF Relocation & Object Generation (Worklet 3: writeElf) <<<\n";
    if (!w3::buildRelocs(model, err))                 { std::cerr << err << "\n"; return 1; }
    if (!w3::writeElf(model, opt.out, err))           { std::cerr << "ELF Writer Error: " << err << "\n"; return 1; }
    if (!w3::checkElf(opt.out, err))                  { std::cerr << "ELF Self-Check Validation Failed: " << err << "\n"; return 1; }

    std::cout << "wrote " << opt.out << " (ELF32 relocatable): .text " << bytes.size() << " bytes, .data "
              << split.sections.dataBytes.size() << " bytes, .bss " << split.sections.bssSize << " bytes, "
              << relocs.size() + split.dataRelocs.size() << " relocation(s)\n";

    if (opt.dumpElf || opt.w3Only) {
        std::cout << "\n";
        w3::dumpElf(opt.out);
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    Options opt;
    std::string input, format;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if      (a == "-o"        && i + 1 < argc) opt.out = argv[++i];
        else if (a == "--format"  && i + 1 < argc) format = argv[++i];
        else if (a == "--base"    && i + 1 < argc) opt.base = static_cast<uint32_t>(std::strtoul(argv[++i], 0, 0));
        else if (a == "--bin"     && i + 1 < argc) opt.alsoBin = argv[++i];
        else if (a == "--listing")                 opt.listing = true;
        else if (a == "--w1" || a == "--w1-only")  opt.w1Only = true;
        else if (a == "--w2" || a == "--w2-only")  opt.w2Only = true;
        else if (a == "--w3" || a == "--w3-only")  opt.w3Only = true;
        else if (a == "--dump-elf")                opt.dumpElf = true;
        else if (!a.empty() && a[0] == '-')        { std::cerr << "Unknown option: " << a << "\n"; return 1; }
        else if (positional == 0)                  { input = a; ++positional; }
        else if (positional == 1)                  { opt.out = a; ++positional; }
        else if (positional == 2)                  { opt.base = static_cast<uint32_t>(std::strtoul(a.c_str(), 0, 0)); ++positional; }
    }
    if (input.empty()) {
        std::cerr << "Usage: " << argv[0] << " <file.asm> [options]\n\n"
                  << "Options:\n"
                  << "  -o <path>          Output file path (.o or .bin)\n"
                  << "  --format elf|bin   Output format (default: inferred from extension, else elf)\n"
                  << "  --base <address>   Base load address for flat binary\n"
                  << "  --bin <path>       Additionally write raw .text machine code to <path>\n"
                  << "  --listing          Print disassembly listing with byte offsets\n"
                  << "  --w1, --w1-only    Run Worklet 1 only (tokenize, parse, display IR & labels)\n"
                  << "  --w2, --w2-only    Run Worklet 2 only (encode to machine code, display listing)\n"
                  << "  --w3, --w3-only    Run Worklet 3 only (build ELF and dump header & section table)\n"
                  << "  --dump-elf         Dump ELF Header and Section Headers table (like readelf)\n";
        return 1;
    }
    if (format == "bin" || opt.w2Only) opt.flat = true;
    else if (format == "elf")          opt.flat = false;
    else if (format.empty())           opt.flat = opt.out.size() > 4 && opt.out.compare(opt.out.size() - 4, 4, ".bin") == 0;
    else { std::cerr << "Unknown format '" << format << "' (use elf or bin)\n"; return 1; }

    if (opt.out.empty()) {
        if (opt.w1Only)           opt.out = "";
        else if (opt.flat)        opt.out = "out.bin";
        else                      opt.out = "out.o";
    }

    std::vector<std::string> rawLines;
    if (!readSourceFile(input, rawLines)) { std::cerr << "Error: could not open '" << input << "'\n"; return 1; }
    std::cout << "[Main: main] Input: \"" << input << "\" (" << rawLines.size() << " lines). Target: \"" << opt.out << "\"\n";
    return assemble(rawLines, opt);
}
