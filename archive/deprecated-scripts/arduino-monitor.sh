#!/bin/bash
# Arduino Serial Monitor for WSL2
# Streams serial output directly to WSL2 terminal

ARDUINO_CLI_PATH="/mnt/c/arduino-cli/arduino-cli.exe"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default settings
BAUDRATE="115200"
PORT=""
RAW_MODE="--raw"  # Default to raw mode for clean output

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --baud|-b)
            BAUDRATE="$2"
            shift 2
            ;;
        --formatted|-f)
            RAW_MODE=""
            shift
            ;;
        --help|-h)
            echo "Usage: arduino-monitor [PORT] [OPTIONS]"
            echo "Options:"
            echo "  --baud <rate>    Set baudrate (default: 115200)"
            echo "  --formatted      Show timestamps (default: raw mode)"
            echo "  --help           Show this help message"
            echo ""
            echo "Examples:"
            echo "  arduino-monitor              # Auto-detect port"
            echo "  arduino-monitor COM3         # Specific port"
            echo "  arduino-monitor --baud 9600  # Custom baudrate"
            exit 0
            ;;
        COM*)
            PORT="$1"
            shift
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            shift
            ;;
    esac
done

# Header
echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║    Arduino Serial Monitor for WSL2    ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"

# Auto-detect port if not specified
if [ -z "$PORT" ]; then
    echo "Auto-detecting Arduino board..."
    PORT=$($ARDUINO_CLI_PATH board list --format json 2>/dev/null | grep -o '"port":"COM[0-9]*"' | head -1 | cut -d'"' -f4)
    
    if [ -z "$PORT" ]; then
        echo -e "${RED}Error: No Arduino board detected${NC}"
        echo ""
        echo "Available ports:"
        $ARDUINO_CLI_PATH board list
        echo ""
        echo "Specify port manually: arduino-monitor COM3"
        exit 1
    fi
fi

echo -e "Port: ${GREEN}$PORT${NC}"
echo -e "Baudrate: ${GREEN}$BAUDRATE${NC}"
echo -e "Mode: ${GREEN}$([ -n "$RAW_MODE" ] && echo "Raw" || echo "Formatted")${NC}"
echo ""
echo -e "${YELLOW}Press Ctrl+C to exit${NC}"
echo -e "${CYAN}════════════════════════════════════════${NC}"
echo ""

# Trap Ctrl+C to show clean exit message
trap 'echo -e "\n${CYAN}Serial monitor closed${NC}"; exit 0' INT

# Start monitoring with error handling
if ! $ARDUINO_CLI_PATH monitor -p "$PORT" -c baudrate=$BAUDRATE $RAW_MODE 2>&1; then
    echo -e "${RED}Error: Failed to open serial monitor${NC}"
    echo "Possible issues:"
    echo "1. Port $PORT is in use by another program"
    echo "2. Board was disconnected"
    echo "3. Incorrect baudrate (try --baud 9600)"
    exit 1
fi