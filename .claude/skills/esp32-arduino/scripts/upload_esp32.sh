#!/bin/bash
# ESP32 Upload Script
# Uploads compiled sketches to ESP32 boards

FQBN="esp32:esp32:esp32"
SKETCH_PATH=""
PORT=""
SPEED="921600"

usage() {
    echo "Usage: $0 [OPTIONS] <sketch_path>"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT          Serial port (default: auto-detect)"
    echo "  -s, --speed SPEED        Upload speed in baud (default: 921600)"
    echo "  -f, --fqbn FQBN          Board FQBN (default: esp32:esp32:esp32)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 my_sketch.ino"
    echo "  $0 --port /dev/ttyUSB0 my_sketch.ino"
    echo "  $0 --speed 115200 --port /dev/ttyUSB0 my_sketch.ino"
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -s|--speed)
            SPEED="$2"
            shift 2
            ;;
        -f|--fqbn)
            FQBN="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            SKETCH_PATH="$1"
            shift
            ;;
    esac
done

# Validate sketch path
if [ -z "$SKETCH_PATH" ]; then
    echo "❌ Error: Sketch path required"
    usage
fi

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
        echo "   Please specify port with --port option"
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

# Check port permissions
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo "⚠️  Warning: Insufficient permissions for $PORT"
    echo "   Run: sudo chmod 666 $PORT"
    echo "   Or add yourself to dialout group: sudo usermod -a -G dialout $USER"
    echo ""
fi

echo "📤 Uploading to ESP32..."
echo "   Sketch: $SKETCH_PATH"
echo "   Port: $PORT"
echo "   Speed: $SPEED baud"
echo "   FQBN: $FQBN"
echo ""
echo "   If upload fails, try:"
echo "   1. Hold BOOT button on ESP32"
echo "   2. Press and release EN/RESET button"
echo "   3. Release BOOT button after 'Connecting...' appears"
echo ""

# Perform upload
arduino-cli upload --fqbn "$FQBN" --port "$PORT" --verbose "$SKETCH_PATH"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Upload successful!"
    echo ""
    echo "To monitor serial output, run:"
    echo "   arduino-cli monitor -p $PORT -c baudrate=115200"
    echo "   or use: ./monitor_serial.sh $PORT"
else
    echo ""
    echo "❌ Upload failed!"
    echo ""
    echo "Troubleshooting:"
    echo "  - Ensure the board is connected"
    echo "  - Try holding BOOT button during upload"
    echo "  - Try slower speed: $0 --speed 115200 $SKETCH_PATH"
    echo "  - Check port permissions: ls -la $PORT"
    exit 1
fi
