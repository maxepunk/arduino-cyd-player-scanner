#!/bin/bash
# Arduino CLI Wrapper for WSL2
# Provides seamless integration between WSL2 and Windows Arduino CLI

ARDUINO_CLI_PATH="/mnt/c/arduino-cli/arduino-cli.exe"

# Verify Arduino CLI exists
if [ ! -f "$ARDUINO_CLI_PATH" ]; then
    echo "Error: Arduino CLI not found at $ARDUINO_CLI_PATH" >&2
    echo "Please ensure Arduino CLI is installed in C:\\arduino-cli\\" >&2
    exit 1
fi

# Function to convert paths
convert_path() {
    local path="$1"
    # If it's a file or directory that exists in WSL2, convert it
    if [[ -e "$path" ]]; then
        wslpath -w "$path" 2>/dev/null || echo "$path"
    else
        echo "$path"
    fi
}

# Process arguments and convert file paths
args=()
for arg in "$@"; do
    # Check if argument looks like a path and exists
    if [[ "$arg" == /* ]] || [[ "$arg" == ./* ]] || [[ -e "$arg" ]]; then
        converted=$(convert_path "$arg")
        args+=("$converted")
    else
        args+=("$arg")
    fi
done

# Debug output (comment out in production)
# echo "Debug: Running: $ARDUINO_CLI_PATH ${args[@]}" >&2

# Execute Arduino CLI with converted arguments
"$ARDUINO_CLI_PATH" "${args[@]}"
exit_code=$?

# Return the same exit code as Arduino CLI
exit $exit_code