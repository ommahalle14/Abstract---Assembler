#!/usr/bin/env python3
"""
run_unit_tests.py
Automated Python Runner for Worklet 1 Unit Test Suite.
Builds and executes test_w1_functions.cpp directly in memory.
"""

import os
import sys
import subprocess
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
CPP_FILE = os.path.join(SCRIPT_DIR, "test_w1_functions.cpp")
W2_CPP = os.path.join(PARENT_DIR, "worklet2.cpp")
EXE_FILE = os.path.join(SCRIPT_DIR, "test_w1_functions.exe")

def compile_tests():
    print("[Build] Compiling Worklet 1 unit test runner...")
    cmd = [
        "g++", "-std=c++17", "-O2",
        f"-I{PARENT_DIR}",
        CPP_FILE,
        W2_CPP,
        "-o", EXE_FILE
    ]
    res = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True)
    if res.returncode != 0:
        print("[Build Error] Failed to compile test_w1_functions.cpp:")
        print(res.stderr)
        sys.exit(1)
    print("[Build] Successfully compiled test_w1_functions.exe\n")

def run_tests():
    if not os.path.exists(EXE_FILE) or "--rebuild" in sys.argv:
        compile_tests()

    print("[Execute] Running Worklet 1 Unit Tests...\n")
    start = time.time()
    res = subprocess.run([EXE_FILE], cwd=SCRIPT_DIR, text=True)
    duration = time.time() - start

    print(f"\n[Execution Completed in {duration:.2f} seconds with exit code {res.returncode}]")
    sys.exit(res.returncode)

if __name__ == "__main__":
    run_tests()
