#!/usr/bin/env python

import os
import re
import argparse
import platform
import subprocess
import sys
import tempfile
import warnings
from contextlib import chdir
from distutils.dir_util import copy_tree, remove_tree

# Utility functions
def run_command(command : str, cout: bool = False):
    """Run a shell command and return its output."""
    try:
        print(command)
        result = subprocess.run(command.split(), text=True, capture_output=not cout, check=True)
        if (not cout):
            return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {command}")
        print(e.stderr)
        sys.exit(1)

def find_file(name : str, path : str):
    for root, dirs, files in os.walk(path):
        if name in files:
            return os.path.join(root, name)

def rm_dir(path : str):
    try:
        remove_tree(path)
    except:
        print(f"Directory {path} does not exist.")

def execs_in_dir(directory : str):
    return [
        entry.name
        for entry in os.scandir(directory)
        if entry.is_file() and (entry.path.lower().endswith(".exe") if platform.system() == "Windows" else os.access(entry.path, os.X_OK))
    ]

# Main workflow
if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="top", description="Parse filters")
    parser.add_argument("-f", "--filter", nargs="?", default=".*", help="filter regex for benchmark files")
    args = parser.parse_args()

    if not args.filter.startswith(".*"):
        args.filter = ".*" + args.filter

    if not args.filter.endswith(".*"):
        args.filter = args.filter + ".*"

    print("benchmarks filter: ", args.filter)

    # main
    root = run_command("git rev-parse --show-toplevel")

    # Create benchmarks and copy them to tmp folder.
    with chdir(root):
        bm_baseline_branch = "master"
        bm_contender_branch = run_command("git rev-parse --abbrev-ref HEAD")

        if bm_contender_branch == bm_baseline_branch:
            print("\033[93m" + "WARNING: running benchmarks on the same branch: " + bm_baseline_branch + "\033[0m")

        tmp_dir = tempfile.gettempdir()
        bm_baseline_dir = tmp_dir + "/baseline_benchmark"
        bm_contender_dir = tmp_dir + "/contender_benchmark"

        cfg_cmd = "cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G Ninja -DUMS_BUILD_BENCHMARK=1"
        build_cmd = "cmake --build build --config Release"
        run_command(cfg_cmd)
        run_command(build_cmd)

        rm_dir(bm_contender_dir)

        copy_tree("build/bin/Release/benchmark", bm_contender_dir)

        run_command("git checkout " + bm_baseline_branch)

        run_command(cfg_cmd)
        run_command(build_cmd)

        rm_dir(bm_baseline_dir)

        copy_tree("build/bin/Release/benchmark", bm_baseline_dir)

        run_command("git checkout " + bm_contender_branch)

    # Compare benchmarks.
    with chdir(tmp_dir):
        compare_py_script = find_file("compare.py", root + "/build")

        # Running benchmarks
        for exe in execs_in_dir(bm_baseline_dir):
            if not re.match(args.filter, exe):
                continue

            bm_baseline_exe = bm_baseline_dir + "/" + exe
            bm_contender_exe = bm_contender_dir + "/" + exe
            run_command(compare_py_script + " benchmarks " + bm_baseline_exe + " " + bm_contender_exe, cout=True)
        
        remove_tree(bm_baseline_dir)
        remove_tree(bm_contender_dir)
