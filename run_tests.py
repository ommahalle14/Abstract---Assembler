#!/usr/bin/env python3
"""
run_tests.py
Automated Test Runner for the x86-32 Modular Assembler.
Executes all 100 test cases in 'tests/' in one go:
  - 60 Valid test cases (test_001 to test_060)
  - 40 Error test cases (test_061 to test_100)

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
ASSEMBLER_EXE = ".\\assembler.exe"

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

        # Extract title comment from line 1
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

            rf.write("--------------------------------------------------------------------------------\n")
            rf.write("2. WORKLET 1 OUTPUT (Mode: --w1 [Tokenize, Parse directly into IR, Display IR])\n")
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
            rf.write("3. WORKLET 2 OUTPUT (Mode: --w2 [Encode to Machine Code & Disassembly Listing])\n")
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
            rf.write("4. WORKLET 3 & FULL PIPELINE OUTPUT (Mode: -o --bin --listing --dump-elf)\n")
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
            rf.write("5. WORKLET WORKFLOW SUMMARY\n")
            rf.write("--------------------------------------------------------------------------------\n")
            rf.write(f"- Worklet 1 Status: {'SUCCESS' if w1_rc == 0 else 'ERROR / STOPPED'}\n")
            rf.write(f"- Worklet 2 Status: {'SUCCESS' if w2_rc == 0 else 'ERROR / SKIPPED'}\n")
            rf.write(f"- Worklet 3 Status: {'SUCCESS' if pipe_rc == 0 else 'ERROR / DETECTED'}\n")
            rf.write(f"- Verification Note: {status_note}\n")
            rf.write("================================================================================\n")

        # Console Progress Line
        sym = "[PASS]" if test_status == "PASS" else "[FAIL]"
        cat = "VALID" if is_valid_test else "ERROR"
        print(f"[{test_num:03d}/100] {sym} ({cat}) {test_file:<15} : {status_note}")

    # Generate Master Consolidated Summary
    summary_file_path = os.path.join(RESPONSES_DIR, "all_tests_summary.txt")
    total_tests = len(summary_records)
    total_passed = total_valid_passed + total_error_passed
    total_time = time.time() - start_time

    with open(summary_file_path, "w", encoding="utf-8") as sf:
        sf.write("================================================================================\n")
        sf.write("               X86-32 MODULAR ASSEMBLER - 100 TEST SUITE SUMMARY                \n")
        sf.write("================================================================================\n")
        sf.write(f"Execution Date & Time : {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        sf.write(f"Execution Duration    : {total_time:.2f} seconds\n")
        sf.write(f"Total Tests Executed  : {total_tests}\n")
        sf.write(f"Valid Tests (001-060) : {total_valid_passed}/{total_valid_tests} Passed\n")
        sf.write(f"Error Tests (061-100) : {total_error_passed}/{total_error_tests} Correctly Caught\n")
        sf.write(f"Overall Test Score    : {total_passed}/{total_tests} ({100.0 * total_passed / total_tests:.1f}%)\n")
        sf.write("================================================================================\n\n")

        sf.write("MASTER TEST RESULTS TABLE:\n")
        sf.write(f"{'ID':<4} | {'Test File':<15} | {'Type':<6} | {'Status':<6} | {'Stage / Handled By':<35} | {'Notes / Error'}\n")
        sf.write("-" * 120 + "\n")
        for rec in summary_records:
            sf.write(f"{rec['num']:03d}  | {rec['file']:<15} | {rec['type']:<6} | {rec['status']:<6} | {rec['worklet']:<35} | {rec['note']}\n")
        sf.write("-" * 120 + "\n\n")

        sf.write("================================================================================\n")
        sf.write("DETAILED WORKLET OUTPUTS FOR EACH OF THE 100 TEST CODES\n")
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
