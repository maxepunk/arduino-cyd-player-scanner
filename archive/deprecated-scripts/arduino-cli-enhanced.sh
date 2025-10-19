#!/bin/bash
# Enhanced Arduino CLI Wrapper for WSL2 - v2.0
# Fixes UNC path issues by using Windows temp directory for builds

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

# Get Windows username for temp path
WIN_USER=$(cmd.exe /c "echo %USERNAME%" 2>/dev/null | tr -d '\r\n')
if [ -z "$WIN_USER" ]; then
    # Fallback to WSL username if can't get Windows username
    WIN_USER="$USER"
fi

# Process arguments and convert file paths
args=()
BUILD_PATH_SPECIFIED=false
IS_COMPILE_COMMAND=false

for arg in "$@"; do
    # Check if this is a compile command
    if [[ "$arg" == "compile" ]]; then
        IS_COMPILE_COMMAND=true
        args+=("$arg")
    # Check if build-path is already specified
    elif [[ "$arg" == "--build-path" ]] || [[ "$arg" == "--output-dir" ]]; then
        BUILD_PATH_SPECIFIED=true
        args+=("$arg")
    # Convert paths that exist
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
        # Fallback to generic Windows temp
        TEMP_BUILD="/mnt/c/Windows/Temp/arduino-build-wsl2"
        mkdir -p "$TEMP_BUILD" 2>/dev/null
    fi
    
    if [ -d "$TEMP_BUILD" ]; then
        # Convert to Windows path and add to arguments
        WIN_TEMP_BUILD=$(wslpath -w "$TEMP_BUILD")
        args+=("--build-path" "$WIN_TEMP_BUILD")
        
        # Inform user (can be commented out for production)
        echo "Note: Using build directory: $TEMP_BUILD" >&2
    fi
fi

# Debug output (comment out in production)
# echo "Debug: Running: $ARDUINO_CLI_PATH ${args[@]}" >&2

# Execute Arduino CLI with converted arguments
"$ARDUINO_CLI_PATH" "${args[@]}"
exit_code=$?

# Return the same exit code as Arduino CLI
exit $exit_code