#!/bin/bash
# Production Arduino CLI Wrapper for WSL2 - v2.1
# Fixes UNC path issues with conditional filtering
# Only filters compile output, preserves real-time for monitor/upload

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
    if [[ -e "$path" ]]; then
        wslpath -w "$path" 2>/dev/null || echo "$path"
    else
        echo "$path"
    fi
}

# Get Windows username for temp path
WIN_USER=$(cmd.exe /c "echo %USERNAME%" 2>/dev/null | tr -d '\r\n')
if [ -z "$WIN_USER" ]; then
    WIN_USER="$USER"
fi

# Process arguments and convert file paths
args=()
BUILD_PATH_SPECIFIED=false
IS_COMPILE_COMMAND=false
COMMAND_TYPE=""

for arg in "$@"; do
    # Identify the command type (first non-flag argument)
    if [[ -z "$COMMAND_TYPE" ]] && [[ "$arg" != -* ]]; then
        COMMAND_TYPE="$arg"
    fi
    
    if [[ "$arg" == "compile" ]]; then
        IS_COMPILE_COMMAND=true
        args+=("$arg")
    elif [[ "$arg" == "--build-path" ]] || [[ "$arg" == "--output-dir" ]]; then
        BUILD_PATH_SPECIFIED=true
        args+=("$arg")
    elif [[ "$arg" == /* ]] || [[ "$arg" == ./* ]] || [[ -e "$arg" ]]; then
        converted=$(convert_path "$arg")
        args+=("$converted")
    else
        args+=("$arg")
    fi
done

# If this is a compile command and no build-path specified, add one
if [ "$IS_COMPILE_COMMAND" = true ] && [ "$BUILD_PATH_SPECIFIED" = false ]; then
    # Create a Windows temp build directory
    TEMP_BUILD="/mnt/c/Users/${WIN_USER}/AppData/Local/Temp/arduino-build-wsl2"
    
    # Ensure the directory exists
    if ! mkdir -p "$TEMP_BUILD" 2>/dev/null; then
        TEMP_BUILD="/mnt/c/Windows/Temp/arduino-build-wsl2"
        mkdir -p "$TEMP_BUILD" 2>/dev/null
    fi
    
    if [ -d "$TEMP_BUILD" ]; then
        WIN_TEMP_BUILD=$(wslpath -w "$TEMP_BUILD")
        args+=("--build-path" "$WIN_TEMP_BUILD")
    fi
fi

# Execute with conditional filtering based on command type
if [ "$COMMAND_TYPE" = "compile" ]; then
    # For compile: filter UNC warnings but preserve all other output
    # Using specific patterns to avoid filtering real errors
    "$ARDUINO_CLI_PATH" "${args[@]}" 2>&1 | while IFS= read -r line; do
        # Skip UNC warning lines and the path that triggers them
        if [[ "$line" == "CMD.EXE was started with the above path"* ]] || \
           [[ "$line" == "UNC paths are not supported."* ]] || \
           [[ "$line" =~ ^\'\\\\wsl ]]; then
            continue
        fi
        # Print everything else
        echo "$line"
    done
    # Capture exit code from Arduino CLI (not the while loop)
    exit ${PIPESTATUS[0]}
else
    # For all other commands: no filtering, preserve real-time output
    "$ARDUINO_CLI_PATH" "${args[@]}"
    exit $?
fi