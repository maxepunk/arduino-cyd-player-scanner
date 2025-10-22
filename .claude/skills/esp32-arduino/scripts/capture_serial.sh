#!/bin/bash
# ESP32 Serial Capture Script
# Captures serial output to a file with proper cleanup

PORT=""
BAUDRATE="115200"
OUTPUT_FILE=""
DURATION="30"
RESET_WAIT="2"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT          Serial port (default: auto-detect)"
    echo "  -b, --baudrate RATE      Baudrate (default: 115200)"
    echo "  -o, --output FILE        Output file (default: serial_capture_TIMESTAMP.log)"
    echo "  -d, --duration SECONDS   Capture duration (default: 30)"
    echo "  -w, --wait SECONDS       Wait before capture (default: 2, allows manual reset)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  # Capture for 10 seconds after manual reset"
    echo "  $0 --duration 10"
    echo ""
    echo "  # Save to specific file"
    echo "  $0 --output boot_log.txt --duration 15"
    echo ""
    echo "  # Quick test - capture 5 seconds immediately"
    echo "  $0 --duration 5 --wait 0"
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -b|--baudrate)
            BAUDRATE="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -d|--duration)
            DURATION="$2"
            shift 2
            ;;
        -w|--wait)
            RESET_WAIT="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Auto-detect port if not specified
if [ -z "$PORT" ]; then
    for p in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        if [ -e "$p" ]; then
            PORT="$p"
            break
        fi
    done
    
    if [ -z "$PORT" ]; then
        echo "❌ Error: No serial port detected"
        echo "   Specify port with --port option"
        exit 1
    fi
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Error: Port does not exist: $PORT"
    exit 1
fi

# Generate output filename if not specified
if [ -z "$OUTPUT_FILE" ]; then
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    OUTPUT_FILE="serial_capture_${TIMESTAMP}.log"
fi

echo "📡 ESP32 Serial Capture"
echo "   Port: $PORT"
echo "   Baudrate: $BAUDRATE"
echo "   Output: $OUTPUT_FILE"
echo "   Duration: ${DURATION}s"
echo ""

# Clean up any existing processes using the port
fuser -k "$PORT" 2>/dev/null || true
sleep 0.5

# Configure serial port
stty -F "$PORT" "$BAUDRATE" raw -echo -echoe -echok 2>/dev/null || {
    echo "⚠️  Warning: Could not configure port settings"
}

# Function to cleanup on exit
cleanup() {
    if [ -n "$CAT_PID" ] && kill -0 "$CAT_PID" 2>/dev/null; then
        kill "$CAT_PID" 2>/dev/null || true
    fi
    
    # Show capture results
    if [ -f "$OUTPUT_FILE" ]; then
        FILE_SIZE=$(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo "0")
        LINE_COUNT=$(wc -l < "$OUTPUT_FILE" 2>/dev/null || echo "0")
        
        echo ""
        echo "📊 Capture Complete"
        echo "   File: $OUTPUT_FILE"
        echo "   Size: $FILE_SIZE bytes"
        echo "   Lines: $LINE_COUNT"
        
        if [ "$FILE_SIZE" -eq 0 ]; then
            echo ""
            echo "⚠️  WARNING: No data captured!"
            echo "   Possible issues:"
            echo "   - ESP32 is not transmitting data"
            echo "   - Wrong baudrate (ESP32 using: $BAUDRATE?)"
            echo "   - Serial cable issue"
            echo "   - ESP32 not booting properly"
        else
            echo ""
            echo "✅ Data captured successfully"
            echo ""
            echo "First 20 lines:"
            echo "════════════════════════════════════════"
            head -20 "$OUTPUT_FILE" 2>/dev/null || true
            echo "════════════════════════════════════════"
            
            if [ "$LINE_COUNT" -gt 20 ]; then
                echo ""
                echo "Last 10 lines:"
                echo "════════════════════════════════════════"
                tail -10 "$OUTPUT_FILE" 2>/dev/null || true
                echo "════════════════════════════════════════"
            fi
            
            echo ""
            echo "To view full log: cat $OUTPUT_FILE"
        fi
    fi
}

trap cleanup EXIT INT TERM

# Wait period (allows time for manual reset)
if [ "$RESET_WAIT" -gt 0 ]; then
    echo "⏱️  Waiting ${RESET_WAIT}s before capture..."
    echo "   Press RESET on ESP32 board NOW if you want to capture boot sequence"
    sleep "$RESET_WAIT"
fi

echo ""
echo "🎥 Capturing for ${DURATION}s..."
echo "   (Press Ctrl+C to stop early)"
echo ""

# Start capture in background
cat "$PORT" > "$OUTPUT_FILE" 2>&1 &
CAT_PID=$!

# Wait for specified duration
sleep "$DURATION"

# Cleanup will happen via trap
exit 0
