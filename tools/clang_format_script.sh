#!/bin/bash

# Script that formats all changed/new C++ files that are tracked by git.
# It usses clang-format for formatting.

CLANG_FORMAT_EXE=""
CLANG_FORMAT_FILE=""

# Detect if running on Windows or Unix-based system
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
  # On Windows (MSYS2, Cygwin, or Git Bash), use 'where' command to find clang-format
  CLANG_FORMAT_EXE=$(where clang-format 2> /dev/null)
  # Convert script directory to Windows path if necessary
  CLANG_FORMAT_FILE=$(cygpath -w "$(dirname "$(realpath "$0")")\\.clang-format")
else
  # On Unix-based systems, use 'which' command to find clang-format
  CLANG_FORMAT_EXE=$(which clang-format 2> /dev/null)
  # Use Unix-style path for script directory
  CLANG_FORMAT_FILE=$(dirname "$(realpath "$0")/.clang-format")
fi

# Check if clang-format is installed
if [[ -z "$CLANG_FORMAT_EXE" ]]; then
  echo "Error: clang-format is not installed. Please install it."
  exit 1
fi

# Get the root directory of the Git repository
REPO_ROOT=$(git rev-parse --show-toplevel)

# Change to the root directory of the repository
cd "$REPO_ROOT" || exit

# Get the list of modified, staged, and untracked files
CHANGED_FILES=$(git status -uall --porcelain | grep -E '^\s*[AM]\s.*\.(cpp|hpp|h|c|cc)$' | awk '{print $2}')
NEW_FILES=$(git status -uall --porcelain | grep -E '^\?\?\s.*\.(cpp|hpp|h|c|cc)$' | awk '{print $2}')

# Combine changed and new files and remove duplicates
ALL_FILES=$(echo "$CHANGED_FILES"$'\n'"$NEW_FILES" | awk '!seen[$0]++')

# Check if there are any changed or new C++ files
if [[ -z "$ALL_FILES" ]]; then
  echo "No changed, new, or untracked C++ files to format."
  exit 0
fi

# Convert paths to Windows format if necessary
convert_path() {
  if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    # Convert Unix-style path to Windows-style path
    echo "$1" | sed 's#/#\\#g'
  else
    # Return Unix-compatible path
    echo "$1"
  fi
}

# Arguments for clang-format:
# -i                         -> Change inplace all files.
# --style=file:.clang-format -> use .clang-format file for formatting.
CLANG_FORMAT_ARGS="-i --style=file:$CLANG_FORMAT_FILE"

# Run clang-format on each changed or new file
while IFS= read -r FILE; do
  # Convert file paths for Windows compatibility
  FILE_FOR_FORMATTING=$(convert_path "$REPO_ROOT/$FILE")
  echo "Formatting $FILE_FOR_FORMATTING..."

  COMMAND="$CLANG_FORMAT_EXE $CLANG_FORMAT_ARGS $FILE_FOR_FORMATTING"
  $COMMAND
done <<< "$ALL_FILES"

echo "Formatting completed!"
