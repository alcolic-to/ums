#!/bin/bash

# Script that formats all changed/new/renamed C++ files that are tracked by git.
# It usses clang-format for formatting.

# Clang format setup.
# clang-format args:
# -i                         -> Change inplace all files.
# --style=file:.clang-format -> use .clang-format file for formatting.
CLANG_FORMAT_EXE=""
CLANG_FORMAT_FILE=".clang-format"
CLANG_FORMAT_ARGS="-i --style=file:$CLANG_FORMAT_FILE"

# Detect if running on Windows or Unix-based system
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
  # On Windows (MSYS2, Cygwin, or Git Bash), use 'where' command to find clang-format
  CLANG_FORMAT_EXE=$(where clang-format 2> /dev/null | head -n 1)
else
  # On Unix-based systems, use 'which' command to find clang-format
  CLANG_FORMAT_EXE=$(which clang-format 2> /dev/null)
fi

# Check if clang-format is installed
if [[ -z "$CLANG_FORMAT_EXE" ]]; then
  echo "Error: clang-format is not installed. Please install it."
  exit 1
fi

echo "Found clang-format: ${CLANG_FORMAT_EXE}"

# Get the root directory of the Git repository
REPO_ROOT=$(git rev-parse --show-toplevel)

# Change to the root directory of the repository
cd "$REPO_ROOT" || exit

# Getting all cpp files except deleted ones.
FILES=$(git status -uall --porcelain | grep -E "^[^D].*\.(cpp|hpp|h|c|cc)$" | cut -c 4- | tr '\n' ' ')

# Check if there are any changed or new C++ files
if [[ -z "$FILES" ]]; then
  echo "No changed, new, or untracked C++ files to format."
  exit 0
fi

COMMAND="$CLANG_FORMAT_EXE $CLANG_FORMAT_ARGS $FILES"
echo "Formatting command: ${COMMAND}"
$COMMAND

echo "Formatting completed!"
