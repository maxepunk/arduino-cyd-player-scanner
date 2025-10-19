#!/bin/bash
# Smart Arduino Upload Script for WSL2
# Auto-detects COM port and uploads to ESP32

ARDUINO_CLI_PATH="/mnt/c/arduino-cli/arduino-cli.exe"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default FQBN for ESP32
DEFAULT_FQBN="esp32:esp32:esp32"

# Parse command line arguments
FQBN="$DEFAULT_FQBN"
PROJECT_PATH="."
SPECIFIC_PORT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --fqbn)
            FQBN="$2"
            shift 2
            ;;
        --port|-p)
            SPECIFIC_PORT="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: arduino-upload [OPTIONS]"
            echo "Options:"
            echo "  --fqbn <board>   Specify board type (default: esp32:esp32:esp32)"
            echo "  --port <port>    Specify COM port (default: auto-detect)"
            echo "  --help           Show this help message"
            exit 0
            ;;
        *)
            PROJECT_PATH="$1"
            shift
            ;;
    esac
done

# Convert project path to Windows format
if [ -d "$PROJECT_PATH" ]; then
    WIN_PATH=$(wslpath -w "$PROJECT_PATH")
else
    echo -e "${RED}Error: Project directory '$PROJECT_PATH' not found${NC}"
    exit 1
fi

echo -e "${YELLOW}Arduino Upload Tool for WSL2${NC}"
echo "Project: $PROJECT_PATH"
echo "Board: $FQBN"

# Detect or use specified COM port
if [ -n "$SPECIFIC_PORT" ]; then
    PORT="$SPECIFIC_PORT"
    echo -e "Using specified port: ${GREEN}$PORT${NC}"
else
    # Auto-detect COM port
    echo "Auto-detecting Arduino board..."
    PORT=$($ARDUINO_CLI_PATH board list --format json 2>/dev/null | grep -o '"port":"COM[0-9]*"' | head -1 | cut -d'"' -f4)
    
    if [ -z "$PORT" ]; then
        echo -e "${RED}Error: No Arduino board detected${NC}"
        echo "Available COM ports:"
        $ARDUINO_CLI_PATH board list
        echo ""
        echo "Troubleshooting:"
        echo "1. Check USB cable connection"
        echo "2. Try a different USB port"
        echo "3. Verify drivers in Device Manager"
        echo "4. Close Arduino IDE if running"
        exit 1
    fi
    echo -e "Detected board on: ${GREEN}$PORT${NC}"
fi

# Compile before uploading
echo ""
echo "Compiling sketch..."
if ! $ARDUINO_CLI_PATH compile --fqbn "$FQBN" "$WIN_PATH"; then
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

# Upload to board
echo ""
echo "Uploading to $PORT..."
if $ARDUINO_CLI_PATH upload -p "$PORT" --fqbn "$FQBN" "$WIN_PATH"; then
    echo -e "${GREEN}✓ Upload successful!${NC}"
    echo ""
    echo "To monitor serial output, run:"
    echo "  arduino-monitor $PORT"
    exit 0
else
    echo -e "${RED}Upload failed!${NC}"
    echo "Try specifying port manually: arduino-upload --port COM3"
    exit 1
fi