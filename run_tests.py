#!/usr/bin/env python3
"""
run_tests.py
Automated Test Runner for the x86-32 Modular Assembler.
Executes all test cases in 'tests/' in one go:
  - 60 Valid test cases (test_001 to test_060)
  - 20 Error test cases (test_061 to test_080)

For each test:
  - Runs Worklet 1 (--w1 mode)
  - Runs Worklet 2 (--w2 mode)
  - Runs Worklet 3 & Full Pipeline (-o <out.o> --bin <out.bin> --listing --dump-elf)
  - Captures outputs from each worklet
  - Creates individual response files in 'responses/test_XXX_response.txt'
  - Creates a consolidated master summary in 'responses/all_tests_summary.txt'
"""

import os
import sys
import subprocess
import time
import shutil

TESTS_DIR = "tests"
RESPONSES_DIR = "responses"
TEMP_DIR = "temp_out"
ASSEMBLER_EXE = ".\\assembler.exe" if os.name == "nt" else "./assembler.exe"

TEST_TITLES = {
    61: "Test 061 [ERROR]: Lexical error: missing closing bracket ']' in memory operand",
    62: "Test 062 [ERROR]: Lexical error: invalid immediate literal",
    63: "Test 063 [ERROR]: Lexical error: label name is a reserved register keyword",
    64: "Test 064 [ERROR]: Syntax error: unsupported / unknown mnemonic",
    65: "Test 065 [ERROR]: Syntax error: 16-bit register inside memory brackets",
    66: "Test 066 [ERROR]: Syntax error: rel8 branch (LOOP) cannot reference external symbol",
    67: "Test 067 [ERROR]: Syntax error: register subtracted in memory operand",
    68: "Test 068 [ERROR]: Syntax error: missing operand after comma",
    69: "Test 069 [ERROR]: Symbol error: undefined symbol reference",
    70: "Test 070 [ERROR]: Symbol error: declared GLOBAL symbol is never defined",
    71: "Test 071 [ERROR]: Section error: unknown section directive name",
    72: "Test 072 [ERROR]: Directive error: duplicate EQU definition",
    73: "Test 073 [ERROR]: Directive error: EQU definition conflicts with existing label in data",
    74: "Test 074 [ERROR]: Directive error: duplicate label definition in .data section",
    75: "Test 075 [ERROR]: Directive error: unknown directive in data section",
    76: "Test 076 [ERROR]: Directive error: negative repeat count in TIMES directive",
    77: "Test 077 [ERROR]: Directive error: RESB directive requires count operand",
    78: "Test 078 [ERROR]: Directive error: value out of range for DB (-128..255)",
    79: "Test 079 [ERROR]: Directive error: value out of range for DW (-32768..65535)",
    80: "Test 080 [ERROR]: Directive error: division by zero in EQU expression",
}

ERROR_EXPLANATIONS = {
    61: """Subsystem: Worklet 1 Lexer (tokenizeSourceLine)
Failing Line: mov eax, [ebx + 4
Root-Cause & Specification Explanation:
During lexical analysis, the tokenizer encounters the opening bracket '[' indicating the start
of an x86 memory operand. The lexer enters a scanning loop searching for the terminating ']'.
Because the source line ends prematurely without a closing bracket, Worklet 1 emits:
  "Lexical Error: missing closing ']' for memory operand"
A bracket mismatch is a fatal lexical tokenization failure. Worklet 1 rejects the token, halts
AST construction, and stops the pipeline before passing instructions to Worklet 2.""",

    62: """Subsystem: Worklet 1 Lexer (tokenizeSourceLine -> parseNumericLiteral)
Failing Line: mov eax, 12345xyz
Root-Cause & Specification Explanation:
The token '12345xyz' begins with digits, leading the lexer to classify it as a candidate immediate
numeric literal. Worklet 1's multi-radix numeric parser (parseNumericLiteral) evaluates digits in base 10,
but encounters invalid characters ('x', 'y', 'z'). Since it does not conform to valid hexadecimal (0x prefix or h suffix),
binary (b suffix), or octal (o/q suffix), Worklet 1 emits:
  "Lexical Error: invalid immediate '12345xyz'"
Malformed literals are rejected at the token level, preventing invalid constants from propagating into IR.""",

    63: """Subsystem: Worklet 1 Lexer (tokenizeSourceLine)
Failing Line: eax:
Root-Cause & Specification Explanation:
When tokenizing a label definition (an identifier immediately followed by a colon ':'), Worklet 1
verifies whether the symbol name collides with reserved keywords using isValidRegisterName() and
isValidMnemonicName(). 'EAX' is an x86 hardware register. To prevent syntactic ambiguity between
register operands and jump target identifiers, Worklet 1 reports:
  "Lexical Error: label name 'eax' is a reserved word"
Hardware registers and ISA mnemonics cannot be used as label names.""",

    64: """Subsystem: Worklet 1 Parser (parseAssemblySource)
Failing Line: invalidop eax, ebx
Root-Cause & Specification Explanation:
During instruction parsing, the statement's initial token must be a recognized x86-32 mnemonic.
Worklet 1 checks its internal opcode dispatch tables (lookupBaseMnemonic and lookupExtendedMnemonic).
Because 'invalidop' is not present in the x86 ISA vocabulary, Worklet 1 emits:
  "Syntax Error: unsupported mnemonic 'invalidop'"
The parser terminates front-end IR generation without creating a w2::Instruction node.""",

    65: """Subsystem: Worklet 1 Parser (parseAssemblySource -> parseOperand)
Failing Line: mov eax, [bx]
Root-Cause & Specification Explanation:
In 32-bit protected mode (IA-32), effective memory addresses require 32-bit general-purpose registers
(EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI). 16-bit registers ([BX], [BP+SI], etc.) are real-mode addressing
constructs. Worklet 1 inspects the register size returned by lookupRegister() inside memory operands.
Discovering bits == 16 for 'BX', Worklet 1 reports:
  "Syntax Error: 16/8-bit register 'BX' cannot be used as an address (use 32-bit)"
16-bit base/index addressing is rejected in 32-bit flat memory model programs.""",

    66: """Subsystem: Worklet 1 Parser (parseAssemblySource)
Failing Line: loop ext_func
Root-Cause & Specification Explanation:
The x86 'LOOP' instruction is strictly encoded with an 8-bit signed relative displacement (rel8),
constraining the target address to within -128 to +127 bytes of the current instruction pointer.
External symbols declared via 'extern' reside in external compilation units and require 32-bit
relocations (R_386_PC32). Because a rel8 operand cannot accommodate an external link-time relocation,
Worklet 1 enforces ISA relocation constraints and reports:
  "Syntax Error: LOOP cannot reference an external symbol (rel8)"
Short branch instructions (LOOP, LOOPE, LOOPNE, JECXZ) cannot target external symbols.""",

    67: """Subsystem: Worklet 1 Memory Syntax Validator (validateMemoryExpressionSyntax)
Failing Line: mov eax, [ebx-ecx]
Root-Cause & Specification Explanation:
The x86 SIB / ModR/M addressing hardware physically only supports additions between base and index
registers: Effective Address = Base + (Index * Scale) + Displacement. Registers cannot be subtracted
in hardware. Worklet 1's syntax validator checks the mathematical sign preceding register terms.
Upon detecting '-ecx', it emits:
  "Syntax Error: a register cannot be subtracted in a memory operand ('[ebx-ecx]')"
Negative arithmetic is only permitted on numeric displacements, not on register components.""",

    68: """Subsystem: Worklet 1 Parser (parseAssemblySource)
Failing Line: mov eax,
Root-Cause & Specification Explanation:
The 'MOV' instruction requires two operands separated by a comma (destination, source).
After consuming the first register operand 'eax' and the comma token ',', the parser reaches the
end of the statement. Worklet 1 identifies a trailing comma with no subsequent operand and reports:
  "Syntax Error: missing operand after comma"
Instructions cannot end abruptly with a dangling comma.""",

    69: """Subsystem: Worklet 1 Symbol Resolution (parseAssemblySource)
Failing Line: jmp nonexistent_target
Root-Cause & Specification Explanation:
When resolving branch targets and memory labels, Worklet 1 queries the discovered label table
(idByName) and the declared externs set. 'nonexistent_target' has neither been defined as a label
in .text nor declared with an 'extern' directive. Worklet 1 emits:
  "Symbol Error: undefined symbol 'nonexistent_target'"
Unresolved symbols are caught at the front-end parser stage, preventing broken branch offsets.""",

    70: """Subsystem: Worklet 1 Symbol Table Generator (buildSymbolTable)
Failing Line: global undefined_global_func
Root-Cause & Specification Explanation:
In Phase 4, Worklet 1 generates the ELF symbol table (.symtab) and string table (.strtab).
It validates every symbol declared with the 'global' directive against defined section labels and
EQU constants. Because 'undefined_global_func' was declared global but never defined anywhere in
the translation unit, Worklet 1 halts and reports:
  "Symbol Table Error: Global symbol 'undefined_global_func' is declared but not defined"
Every exported global symbol must have a concrete address in .text, .data, or .bss.""",

    71: """Subsystem: Worklet 3 Source Splitting (splitSource)
Failing Line: section .my_custom_section
Root-Cause & Specification Explanation:
Worklet 3 manages sections and directives. The modular assembler only targets standard ELF
sections (.text, .data, .bss). Encountering an unrecognized section header triggers:
  "Syntax Error: unknown section '.MY_CUSTOM_SECTION'"
Non-standard sections are rejected during initial source splitting.""",

    72: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: BUFFER_SIZE equ 2048
Root-Cause & Specification Explanation:
The constant 'BUFFER_SIZE' was already defined earlier in the source (equ 1024).
Worklet 3 enforces immutability on EQU definitions, rejecting duplicate assignment with:
  "Error: duplicate equ definition of 'BUFFER_SIZE'"
EQU constants cannot be redefined within the same source file.""",

    73: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: func_entry equ 42
Root-Cause & Specification Explanation:
The symbol 'func_entry' was already defined as a memory label in the .data section (db 1).
Worklet 3 checks for name collisions between symbols and EQU constants, reporting:
  "Error: symbol 'func_entry' already defined as a label"
A symbol cannot be both a section-relative label and an absolute EQU constant.""",

    74: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: my_data_var: db 2
Root-Cause & Specification Explanation:
The label 'my_data_var' was already defined on the previous line in the .data section.
Worklet 3 tracks data-segment labels and detects the duplicate definition:
  "Error: duplicate label 'my_data_var'"
Labels within the same section must be unique.""",

    75: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: align 4
Root-Cause & Specification Explanation:
The directive 'align' is not implemented in the current assembler grammar.
Worklet 3 encounters the unrecognized token and reports:
  "Syntax Error: unknown directive 'align'"
Unsupported alignment directives halt the splitter before phase 2.""",

    76: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: times -5 db 1
Root-Cause & Specification Explanation:
The repeat count operand of the TIMES directive must evaluate to a positive integer.
Worklet 3 evaluates the expression '-5' and rejects negative repetition counts:
  "Error: negative repeat count -5"
Repetition counts must be >= 0.""",

    77: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: resb_missing: resb
Root-Cause & Specification Explanation:
The RESB (Reserve Byte) directive in .bss requires an operand specifying how many bytes to reserve.
Worklet 3 encounters a blank operand and reports:
  "Syntax Error: 'RESB' directive requires count operand"
Storage reservation directives cannot omit the size count.""",

    78: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: db 500
Root-Cause & Specification Explanation:
The DB (Define Byte) directive emits 8-bit data values, supporting a range of -128 to 255.
Worklet 3 checks the evaluated numeric literal (500) against 8-bit limits and reports:
  "Error: value 500 out of range for db (-128..255)"
Values exceeding 8 bits must use DW or DD.""",

    79: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: dw 100000
Root-Cause & Specification Explanation:
The DW (Define Word) directive emits 16-bit data values, supporting a range of -32768 to 65535.
Worklet 3 checks the evaluated numeric literal (100000) against 16-bit limits and reports:
  "Error: value 100000 out of range for dw (-32768..65535)"
Values exceeding 16 bits must use DD.""",

    80: """Subsystem: Worklet 3 Directive Evaluation (splitSource)
Failing Line: DIV_ZERO equ 100 / 0
Root-Cause & Specification Explanation:
Worklet 3's expression parser evaluates arithmetic operations for EQU constant directives.
When evaluating division, it checks whether the divisor is zero and safely halts with:
  "Error: Division by zero at line 4, column 5"
Division by zero is an invalid mathematical operation.""",
}

def ensure_assembler():
    if not os.path.exists("assembler.exe"):
        print("[Build] assembler.exe not found. Compiling assembler...")
        cmd = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-o", "assembler.exe", "main.cpp", "worklet2.cpp"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print("[Build Error] Failed to compile assembler.exe:")
            print(res.stderr)
            sys.exit(1)
        print("[Build] assembler.exe compiled successfully.\n")

def run_cmd(cmd):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT: Process took longer than 10 seconds."
    except Exception as e:
        return -1, "", f"EXCEPTION: {str(e)}"

def main():
    start_time = time.time()
    ensure_assembler()

    os.makedirs(RESPONSES_DIR, exist_ok=True)
    os.makedirs(TEMP_DIR, exist_ok=True)

    # Discover test files
    test_files = sorted([f for f in os.listdir(TESTS_DIR) if f.startswith("test_") and f.endswith(".asm")])
    if not test_files:
        print(f"[Error] No test files found in '{TESTS_DIR}' directory.")
        sys.exit(1)

    print(f"================================================================================")
    print(f"  RUNNING ALL {len(test_files)} X86-32 ASSEMBLER TESTS IN ONE GO")
    print(f"================================================================================")
    print(f"  Tests Directory     : {os.path.abspath(TESTS_DIR)}")
    print(f"  Responses Directory : {os.path.abspath(RESPONSES_DIR)}")
    print(f"================================================================================\n")

    summary_records = []
    total_valid_passed = 0
    total_valid_tests = 0
    total_error_passed = 0
    total_error_tests = 0

    for idx, test_file in enumerate(test_files, 1):
        test_path = os.path.join(TESTS_DIR, test_file)
        base_name = os.path.splitext(test_file)[0]
        test_num = int(base_name.split("_")[1])

        is_valid_test = (test_num <= 60)
        if is_valid_test:
            total_valid_tests += 1
        else:
            total_error_tests += 1

        with open(test_path, "r", encoding="utf-8") as f:
            source_code = f.read()

        # Extract title comment from line 1 or title dictionary
        if test_num in TEST_TITLES:
            test_title = TEST_TITLES[test_num]
        else:
            first_line = source_code.splitlines()[0] if source_code.splitlines() else ""
            test_title = first_line.lstrip(";").strip()

        # 1. Run Worklet 1
        w1_cmd = [ASSEMBLER_EXE, test_path, "--w1"]
        w1_rc, w1_stdout, w1_stderr = run_cmd(w1_cmd)

        # 2. Run Worklet 2
        w2_bin = os.path.join(TEMP_DIR, f"{base_name}.bin")
        w2_cmd = [ASSEMBLER_EXE, test_path, "--w2", "-o", w2_bin]
        w2_rc, w2_stdout, w2_stderr = run_cmd(w2_cmd)

        # 3. Run Worklet 3 & Full Pipeline
        pipe_o = os.path.join(TEMP_DIR, f"{base_name}.o")
        pipe_bin = os.path.join(TEMP_DIR, f"{base_name}_pipe.bin")
        pipe_cmd = [ASSEMBLER_EXE, test_path, "-o", pipe_o, "--bin", pipe_bin, "--listing", "--dump-elf"]
        pipe_rc, pipe_stdout, pipe_stderr = run_cmd(pipe_cmd)

        # Evaluate correctness
        test_status = "FAIL"
        status_note = ""

        if is_valid_test:
            # Valid test expects full pipeline to succeed (RC == 0)
            if pipe_rc == 0:
                test_status = "PASS"
                status_note = "Valid Assembly Assembled & Linked Successfully"
                total_valid_passed += 1
            else:
                test_status = "FAIL"
                status_note = f"Unexpected Failure in Valid Test: {pipe_stderr.strip().splitlines()[-1] if pipe_stderr.strip() else 'Exit code ' + str(pipe_rc)}"
        else:
            # Error test expects an error (RC != 0)
            if pipe_rc != 0:
                test_status = "PASS"
                err_line = pipe_stderr.strip().splitlines()[0] if pipe_stderr.strip() else "Error detected"
                status_note = f"Expected Error Caught: {err_line}"
                total_error_passed += 1
            else:
                test_status = "FAIL"
                status_note = "Error test unexpectedly assembled with exit code 0!"

        # Identify which worklet caught the error (if error test)
        detected_worklet = "All Passed"
        if not is_valid_test:
            if "Worklet 3: splitSource" in pipe_stdout or "Phase 1: Source Splitting" in pipe_stdout:
                if "Phase 2" not in pipe_stdout:
                    detected_worklet = "Worklet 3 (splitSource / directives)"
                elif "Phase 3" not in pipe_stdout:
                    detected_worklet = "Worklet 1 (lexer / parser / symbols)"
                elif "Phase 4" not in pipe_stdout:
                    detected_worklet = "Worklet 2 (encoder)"
                elif "Phase 5" not in pipe_stdout:
                    detected_worklet = "Worklet 1 (symbol table generator)"
                else:
                    detected_worklet = "Worklet 3 (ELF writer / relocations)"
            else:
                detected_worklet = "Front-End Error"

        summary_records.append({
            "num": test_num,
            "file": test_file,
            "title": test_title,
            "type": "VALID" if is_valid_test else "ERROR",
            "status": test_status,
            "worklet": detected_worklet,
            "note": status_note,
            "w1_rc": w1_rc,
            "w2_rc": w2_rc,
            "pipe_rc": pipe_rc
        })

        # Generate individual response file
        resp_file_path = os.path.join(RESPONSES_DIR, f"{base_name}_response.txt")
        with open(resp_file_path, "w", encoding="utf-8") as rf:
            rf.write("================================================================================\n")
            rf.write(f"TEST CODE RUN RESPONSE: {test_file}\n")
            rf.write("================================================================================\n")
            rf.write(f"Test Number   : {test_num:03d}\n")
            rf.write(f"Test Title    : {test_title}\n")
            rf.write(f"Category      : {'VALID TEST CASE' if is_valid_test else 'ERROR TEST CASE'}\n")
            rf.write(f"Overall Result: {test_status} ({status_note})\n")
            rf.write("================================================================================\n\n")

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write("1. SOURCE CODE\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(source_code.strip() + "\n\n")

            if test_num in ERROR_EXPLANATIONS:
                rf.write("--------------------------------------------------------------------------------\n")
                if 61 <= test_num <= 70:
                    rf.write("2. WORKLET 1 ERROR ROOT-CAUSE ANALYSIS & EXPLANATION\n")
                else:
                    rf.write("2. WORKLET 3 ERROR ROOT-CAUSE ANALYSIS & EXPLANATION\n")
                rf.write("--------------------------------------------------------------------------------\n")
                rf.write(ERROR_EXPLANATIONS[test_num].strip() + "\n\n")

            w1_sec_num = "3" if test_num in ERROR_EXPLANATIONS else "2"
            w2_sec_num = "4" if test_num in ERROR_EXPLANATIONS else "3"
            w3_sec_num = "5" if test_num in ERROR_EXPLANATIONS else "4"
            summary_sec_num = "6" if test_num in ERROR_EXPLANATIONS else "5"

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"{w1_sec_num}. WORKLET 1 OUTPUT (Mode: --w1 [Tokenize, Parse directly into IR, Display IR])\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"Return Code: {w1_rc}\n")
            if w1_stdout.strip():
                rf.write("[STDOUT]:\n" + w1_stdout.strip() + "\n")
            if w1_stderr.strip():
                rf.write("[STDERR]:\n" + w1_stderr.strip() + "\n")
            if not w1_stdout.strip() and not w1_stderr.strip():
                rf.write("(No output produced)\n")
            rf.write("\n")

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"{w2_sec_num}. WORKLET 2 OUTPUT (Mode: --w2 [Encode to Machine Code & Disassembly Listing])\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"Return Code: {w2_rc}\n")
            if w2_stdout.strip():
                rf.write("[STDOUT]:\n" + w2_stdout.strip() + "\n")
            if w2_stderr.strip():
                rf.write("[STDERR]:\n" + w2_stderr.strip() + "\n")
            if not w2_stdout.strip() and not w2_stderr.strip():
                rf.write("(No output produced)\n")
            rf.write("\n")

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"{w3_sec_num}. WORKLET 3 & FULL PIPELINE OUTPUT (Mode: -o --bin --listing --dump-elf)\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"Return Code: {pipe_rc}\n")
            if pipe_stdout.strip():
                rf.write("[STDOUT]:\n" + pipe_stdout.strip() + "\n")
            if pipe_stderr.strip():
                rf.write("[STDERR]:\n" + pipe_stderr.strip() + "\n")
            if not pipe_stdout.strip() and not pipe_stderr.strip():
                rf.write("(No output produced)\n")
            rf.write("\n")

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"{summary_sec_num}. WORKLET WORKFLOW SUMMARY\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"- Worklet 1 Status: {'SUCCESS' if w1_rc == 0 else 'ERROR / STOPPED'}\n")
            rf.write(f"- Worklet 2 Status: {'SUCCESS' if w2_rc == 0 else 'ERROR / SKIPPED'}\n")
            rf.write(f"- Worklet 3 Status: {'SUCCESS' if pipe_rc == 0 else 'ERROR / DETECTED'}\n")
            rf.write(f"- Verification Note: {status_note}\n")
            rf.write("================================================================================\n")

        # Console Progress Line
        sym = "[PASS]" if test_status == "PASS" else "[FAIL]"
        cat = "VALID" if is_valid_test else "ERROR"
        print(f"[{test_num:03d}/{len(test_files):03d}] {sym} ({cat}) {test_file:<15} : {status_note}")

    # Generate Master Consolidated Summary
    summary_file_path = os.path.join(RESPONSES_DIR, "all_tests_summary.txt")
    total_tests = len(summary_records)
    total_passed = total_valid_passed + total_error_passed
    total_time = time.time() - start_time

    with open(summary_file_path, "w", encoding="utf-8") as sf:
        sf.write("================================================================================\n")
        sf.write(f"               X86-32 MODULAR ASSEMBLER - {total_tests} TEST SUITE SUMMARY                \n")
        sf.write("================================================================================\n")
        sf.write(f"Execution Date & Time : {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        sf.write(f"Execution Duration    : {total_time:.2f} seconds\n")
        sf.write(f"Total Tests Executed  : {total_tests}\n")
        sf.write(f"Valid Tests (001-060) : {total_valid_passed}/{total_valid_tests} Passed\n")
        sf.write(f"Error Tests (061-{total_tests:03d}) : {total_error_passed}/{total_error_tests} Correctly Caught\n")
        sf.write(f"Overall Test Score    : {total_passed}/{total_tests} ({100.0 * total_passed / total_tests:.1f}%)\n")
        sf.write("================================================================================\n\n")

        sf.write("MASTER TEST RESULTS TABLE:\n")
        sf.write(f"{'ID':<4} | {'Test File':<15} | {'Type':<6} | {'Status':<6} | {'Stage / Handled By':<35} | {'Notes / Error'}\n")
        sf.write("-" * 120 + "\n")
        for rec in summary_records:
            sf.write(f"{rec['num']:03d}  | {rec['file']:<15} | {rec['type']:<6} | {rec['status']:<6} | {rec['worklet']:<35} | {rec['note']}\n")
        sf.write("-" * 120 + "\n\n")

        sf.write("================================================================================\n")
        sf.write(f"DETAILED WORKLET OUTPUTS FOR EACH OF THE {total_tests} TEST CODES\n")
        sf.write("================================================================================\n\n")

        for rec in summary_records:
            test_resp_file = os.path.join(RESPONSES_DIR, f"{rec['file'].replace('.asm', '')}_response.txt")
            if os.path.exists(test_resp_file):
                with open(test_resp_file, "r", encoding="utf-8") as rf:
                    sf.write(rf.read())
                sf.write("\n\n" + "#" * 80 + "\n\n")

    print("\n" + "=" * 80)
    print("  TEST RUN COMPLETE")
    print("=" * 80)
    print(f"  Valid Tests Passed    : {total_valid_passed}/{total_valid_tests}")
    print(f"  Error Tests Caught    : {total_error_passed}/{total_error_tests}")
    print(f"  Total Score           : {total_passed}/{total_tests} ({(100.0 * total_passed / total_tests):.1f}%)")
    print(f"  Total Run Time        : {total_time:.2f} seconds")
    print(f"  Responses Folder      : {os.path.abspath(RESPONSES_DIR)}")
    print(f"  Master Summary Report : {os.path.abspath(summary_file_path)}")
    print("=" * 80 + "\n")

    # Clean up temporary object files
    if os.path.exists(TEMP_DIR):
        shutil.rmtree(TEMP_DIR, ignore_errors=True)

if __name__ == "__main__":
    main()
