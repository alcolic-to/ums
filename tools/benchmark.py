import os
import platform
import subprocess
import sys
import tempfile
from contextlib import chdir
from distutils.dir_util import copy_tree, remove_tree

# Utility functions
def run_command(command, cwd=None, cout=False):
    """Run a shell command and return its output."""
    try:
        print(f"Running: {command}")
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=not cout, check=True)
        if (not cout):
            return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {command}")
        print(e.stderr)
        sys.exit(1)

def find_file(name, path):
    for root, dirs, files in os.walk(path):
        if name in files:
            return os.path.join(root, name)

def rm_dir(path):
    try:
        remove_tree(path)
    except:
        print(f"Directory {path} does not exist.")

def execs_in_dir(directory):
    return [
        entry.name
        for entry in os.scandir(directory)
        if entry.is_file() and (entry.path.lower().endswith(".exe") if platform.system() == "Windows" else os.access(entry.path, os.X_OK))
    ]

# Main workflow
if __name__ == "__main__":
    cwd = os.getcwd()
    root = run_command("git rev-parse --show-toplevel")

    bm_baseline_branch = "master"
    bm_test_branch = run_command("git rev-parse --abbrev-ref HEAD")

    tmp_dir = tempfile.gettempdir()
    bm_baseline_dir = tmp_dir + "/baseline_benchmark"
    bm_test_dir = tmp_dir + "/test_benchmark"

    # Create benchmarks and copy them to tmp folder.
    with chdir(root):
        cfg_cmd = "cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G Ninja -DUMS_BUILD_BENCHMARK=1"
        build_cmd = "cmake --build build --config Release"
        run_command(cfg_cmd)
        run_command(build_cmd)

        rm_dir(bm_test_dir)

        copy_tree("build/bin/Release/benchmark", bm_test_dir)

        run_command("git checkout " + bm_baseline_branch)

        run_command(cfg_cmd)
        run_command(build_cmd)

        rm_dir(bm_baseline_dir)

        copy_tree("build/bin/Release/benchmark", bm_baseline_dir)

        run_command("git checkout " + bm_test_branch)

    # Compare benchmarks.
    with chdir(tmp_dir):
        compare_py_script = find_file("compare.py", root + "/build")

        # Running benchmarks
        for exe in execs_in_dir(bm_baseline_dir):
            bm_baseline_exe = bm_baseline_dir + "/" + exe
            bm_test_exe = bm_test_dir + "/" + exe
            run_command("py " + compare_py_script + " benchmarks " + bm_baseline_exe + " " + bm_test_exe, cout=True)
        
        remove_tree(bm_baseline_dir)
        remove_tree(bm_test_dir)
    
