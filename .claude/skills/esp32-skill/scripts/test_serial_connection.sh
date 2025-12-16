#!/bin/bash
# ESP32 Serial Connection Test
# Tests if serial port is working and receiving data

PORT=""
BAUDRATE="115200"
TEST_DURATION="5"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT          Serial port (default: auto-detect)"
    echo "  -b, --baudrate RATE      Baudrate (default: 115200)"
    echo "  -d, --duration SECONDS   Test duration (default: 5)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "This script tests if the serial connection is working by:"
    echo "  1. Checking port accessibility"
    echo "  2. Configuring port settings"
    echo "  3. Listening for data"
    echo "  4. Reporting results"
    echo ""
    echo "Examples:"
    echo "  $0                       # Quick 5-second test"
    echo "  $0 --duration 10         # Longer test"
    echo "  $0 --baudrate 9600       # Different baudrate"
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
        -d|--duration)
            TEST_DURATION="$2"
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

echo "🔍 ESP32 Serial Connection Test"
echo ""

# Test 1: Port Detection
echo "Test 1: Port Detection"
echo "─────────────────────────"

if [ -z "$PORT" ]; then
    echo "   Auto-detecting port..."
    for p in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        if [ -e "$p" ]; then
            PORT="$p"
            echo "   ✅ Found: $PORT"
            break
        fi
    done
    
    if [ -z "$PORT" ]; then
        echo "   ❌ FAILED: No serial port detected"
        echo ""
        echo "Available serial devices:"
        ls -la /dev/tty* 2>/dev/null | grep -E "USB|ACM" || echo "   None found"
        echo ""
        echo "Troubleshooting:"
        echo "  - Is ESP32 connected via USB?"
        echo "  - Check USB cable (must be data cable, not charge-only)"
        echo "  - Check dmesg for USB connection: dmesg | tail -20"
        echo "  - May need USB driver (CP210x, CH340, FTDI)"
        exit 1
    fi
else
    if [ -e "$PORT" ]; then
        echo "   ✅ Port exists: $PORT"
    else
        echo "   ❌ FAILED: Port does not exist: $PORT"
        exit 1
    fi
fi

echo ""

# Test 2: Port Permissions
echo "Test 2: Port Permissions"
echo "─────────────────────────"

if [ -r "$PORT" ] && [ -w "$PORT" ]; then
    echo "   ✅ Port is readable and writable"
else
    echo "   ❌ FAILED: Insufficient permissions"
    echo ""
    echo "Current permissions:"
    ls -la "$PORT"
    echo ""
    echo "Fix with:"
    echo "  sudo chmod 666 $PORT           # Temporary"
    echo "  sudo usermod -a -G dialout \$USER  # Permanent (requires logout)"
    exit 1
fi

echo ""

# Test 3: Port Configuration
echo "Test 3: Port Configuration"
echo "─────────────────────────"

# Clean up any existing processes
fuser -k "$PORT" 2>/dev/null || true
sleep 0.5

# Try to configure port
if stty -F "$PORT" "$BAUDRATE" raw -echo -echoe -echok 2>/dev/null; then
    echo "   ✅ Port configured: $BAUDRATE baud"
else
    echo "   ⚠️  WARNING: Could not configure port settings"
    echo "   Continuing anyway..."
fi

# Show current settings
echo ""
echo "Port settings:"
stty -F "$PORT" -a 2>/dev/null | head -2 || echo "   Unable to read settings"

echo ""

# Test 4: Data Reception
echo "Test 4: Data Reception"
echo "─────────────────────────"
echo "   Listening for ${TEST_DURATION}s..."

temp_file=$(mktemp)

# Capture data
timeout "$TEST_DURATION" cat "$PORT" > "$temp_file" 2>&1 &
cat_pid=$!

# Show progress
for ((i=1; i<=$TEST_DURATION; i++)); do
    sleep 1
    size=$(stat -f%z "$temp_file" 2>/dev/null || stat -c%s "$temp_file" 2>/dev/null)
    printf "\r   Progress: %d/%ds (received: %d bytes)" $i $TEST_DURATION $size
done

wait $cat_pid 2>/dev/null
echo ""

# Check results
file_size=$(stat -f%z "$temp_file" 2>/dev/null || stat -c%s "$temp_file" 2>/dev/null)
line_count=$(wc -l < "$temp_file" 2>/dev/null)

if [ "$file_size" -gt 0 ]; then
    echo "   ✅ PASSED: Received $file_size bytes ($line_count lines)"
    echo ""
    echo "Sample data (first 15 lines):"
    echo "════════════════════════════════════════"
    head -15 "$temp_file"
    echo "════════════════════════════════════════"
    
    # Check for common patterns
    echo ""
    echo "Data analysis:"
    
    if grep -qi "rst:" "$temp_file" 2>/dev/null; then
        echo "   ✓ Boot messages detected"
    fi
    
    if grep -qi "error\|fail\|panic" "$temp_file" 2>/dev/null; then
        echo "   ⚠ Error messages detected"
    fi
    
    if grep -qi "WiFi\|connected" "$temp_file" 2>/dev/null; then
        echo "   ✓ WiFi activity detected"
    fi
    
    # Save to file for further analysis
    final_file="serial_test_$(date +%Y%m%d_%H%M%S).log"
    mv "$temp_file" "$final_file"
    echo ""
    echo "Full output saved to: $final_file"
    
else
    echo "   ❌ FAILED: No data received"
    echo ""
    echo "Possible causes:"
    echo "  1. ESP32 is not running"
    echo "  2. Wrong baudrate"
    echo "     - ESP32 code uses: $BAUDRATE?"
    echo "     - Common baudrates: 9600, 115200, 230400"
    echo "     - Try: $0 --baudrate 9600"
    echo "  3. ESP32 crashed or in reset loop"
    echo "  4. Serial TX pin not connected"
    echo "  5. Wrong USB cable (charge-only, not data)"
    echo ""
    echo "Next steps:"
    echo "  1. Try different baudrate: $0 --baudrate 9600"
    echo "  2. Check ESP32 is powered: LED should be on"
    echo "  3. Press RESET button and retry immediately"
    echo "  4. Check dmesg for USB errors: dmesg | tail -20"
    rm -f "$temp_file"
    exit 1
fi

echo ""
echo "═══════════════════════════════════════════"
echo "✅ Serial Connection Test: PASSED"
echo "═══════════════════════════════════════════"
echo ""
echo "The serial connection is working!"
echo ""
echo "Next steps:"
echo "  - Compile and upload: ./compile_esp32.sh sketch.ino"
echo "  - Monitor output: ./monitor_serial.sh"
echo "  - Test commands: ./test_serial_command.sh --command STATUS"
echo "  - Capture boot: ./capture_serial.sh --duration 10"

rm -f "$temp_file"
exit 0
