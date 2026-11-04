#!/usr/bin/env python3
"""
run_unit_tests.py
Automated Unit Test Runner for Worklet 1 Major Functions:
  1. test_parseMemoryExpressionComponents.cpp
  2. test_parseAssemblySource.cpp
  3. test_buildSymbolTable.cpp

Testing Level: Unit Testing (Level 1 in SDLC)
Testing Technique: White Box Testing (Statement, Branch, Path Coverage & Error Guessing)
"""

import os
import sys
import subprocess
import time

TEST_FILES = [
    {
        "source": "test_parseMemoryExpressionComponents.cpp",
        "exe": "test_parseMemoryExpressionComponents.exe",
        "func": "parseMemoryExpressionComponents()",
        "desc": "x86-32 Memory Syntax & SIB Expression Component Parser"
    },
    {
        "source": "test_parseAssemblySource.cpp",
        "exe": "test_parseAssemblySource.exe",
        "func": "parseAssemblySource()",
        "desc": "Front-End Assembly Lexer, IR Generator & Error Diagnostics"
    },
    {
        "source": "test_buildSymbolTable.cpp",
        "exe": "test_buildSymbolTable.exe",
        "func": "buildSymbolTable()",
        "desc": "ELF32 Symbol (.symtab) and String (.strtab) Table Generator"
    }
]

def compile_test(script_dir, test_info, rebuild=False):
    src_path = os.path.join(script_dir, test_info["source"])
    exe_path = os.path.join(script_dir, test_info["exe"])
    worklet2_path = os.path.abspath(os.path.join(script_dir, "..", "worklet2.cpp"))
    parent_dir = os.path.abspath(os.path.join(script_dir, ".."))

    if not rebuild and os.path.exists(exe_path):
        if os.path.getmtime(exe_path) >= os.path.getmtime(src_path):
            return True, ""

    print(f"[Build] Compiling {test_info['source']} -> {test_info['exe']}...")
    cmd = [
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
        f"-I{parent_dir}",
        src_path,
        worklet2_path,
        "-o", exe_path
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        return False, res.stderr
    return True, ""

def run_test(script_dir, test_info):
    exe_name = test_info["exe"]
    exe_path = os.path.join(script_dir, exe_name)
    run_cmd = [f".\\{exe_name}"] if os.name == "nt" else [f"./{exe_name}"]

    proc = subprocess.run(run_cmd, cwd=script_dir, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    rebuild = ("--rebuild" in sys.argv or "-r" in sys.argv)

    print("================================================================================")
    print("      WORKLET 1: MAJOR FUNCTIONS WHITE BOX UNIT TESTING SUITE                   ")
    print("================================================================================")
    print(f"Directory: {script_dir}\n")

    overall_start = time.time()
    results = []

    for idx, test_info in enumerate(TEST_FILES, 1):
        print(f"[{idx}/3] Function: {test_info['func']}")
        print(f"     Description: {test_info['desc']}")
        print(f"     Source File: {test_info['source']}")

        ok, err = compile_test(script_dir, test_info, rebuild)
        if not ok:
            print(f"     [BUILD FAIL]:\n{err}\n")
            results.append((test_info, -1, "", err))
            continue

        rc, stdout, stderr = run_test(script_dir, test_info)
        print(stdout)
        if rc != 0 and stderr:
            print(f"[STDERR]:\n{stderr}")

        results.append((test_info, rc, stdout, stderr))

    total_duration = time.time() - overall_start
    all_passed = all(rc == 0 for _, rc, _, _ in results)

    print("================================================================================")
    print("                       UNIT TEST EXECUTION SUMMARY                              ")
    print("================================================================================")
    for test_info, rc, _, _ in results:
        status = "[PASS]" if rc == 0 else "[FAIL]"
        print(f"  {status} {test_info['func']:<35} ({test_info['source']})")
    print("--------------------------------------------------------------------------------")
    print(f"  Final Suite Outcome : {'ALL UNIT TESTS PASSED [100% SUCCESS]' if all_passed else 'SOME TESTS FAILED'}")
    print(f"  Total Duration      : {total_duration:.2f} seconds")
    print("================================================================================\n")

    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()

