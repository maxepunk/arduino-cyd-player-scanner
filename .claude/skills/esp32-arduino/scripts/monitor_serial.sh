#!/bin/bash
# ESP32 Serial Monitor Script
# Monitors serial output from ESP32 boards

PORT=""
BAUDRATE="115200"

usage() {
    echo "Usage: $0 [OPTIONS] [port]"
    echo ""
    echo "Options:"
    echo "  -b, --baudrate RATE      Baudrate (default: 115200)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Common baudrates: 9600, 115200, 230400, 460800, 921600"
    echo ""
    echo "Examples:"
    echo "  $0                          # Auto-detect port"
    echo "  $0 /dev/ttyUSB0             # Specify port"
    echo "  $0 --baudrate 9600          # Different baudrate"
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--baudrate)
            BAUDRATE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            PORT="$1"
            shift
            ;;
    esac
done

# Auto-detect port if not specified
if [ -z "$PORT" ]; then
    echo "🔍 Auto-detecting ESP32 port..."
    
    # Try common Linux ports
    for p in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        if [ -e "$p" ]; then
            PORT="$p"
            echo "   Found: $PORT"
            break
        fi
    done
    
    if [ -z "$PORT" ]; then
        echo "❌ Error: No serial port detected"
        echo ""
        echo "   Available ports:"
        ls -la /dev/tty* 2>/dev/null | grep -E "USB|ACM" || echo "   None found"
        exit 1
    fi
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Error: Port does not exist: $PORT"
    exit 1
fi

echo "📡 Starting serial monitor..."
echo "   Port: $PORT"
echo "   Baudrate: $BAUDRATE"
echo "   Press Ctrl+C to exit"
echo ""
echo "════════════════════════════════════════════════════════════════"
echo ""

# Use arduino-cli monitor
arduino-cli monitor -p "$PORT" -c baudrate=$BAUDRATE

# Alternative: use screen if arduino-cli monitor has issues
# screen "$PORT" "$BAUDRATE"
