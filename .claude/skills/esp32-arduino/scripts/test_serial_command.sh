#!/bin/bash
# ESP32 Serial Command Test Script
# Sends commands and captures responses for testing

PORT=""
BAUDRATE="115200"
COMMAND=""
TIMEOUT="3"
OUTPUT_FILE=""
INTERACTIVE=0

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT          Serial port (default: auto-detect)"
    echo "  -b, --baudrate RATE      Baudrate (default: 115200)"
    echo "  -c, --command COMMAND    Command to send"
    echo "  -t, --timeout SECONDS    Response timeout (default: 3)"
    echo "  -o, --output FILE        Save response to file"
    echo "  -i, --interactive        Interactive mode (send multiple commands)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  # Send single command and show response"
    echo "  $0 --command \"STATUS\""
    echo ""
    echo "  # Test multiple commands interactively"
    echo "  $0 --interactive"
    echo ""
    echo "  # Send command with longer timeout"
    echo "  $0 --command \"HELP\" --timeout 5"
    echo ""
    echo "  # Save response to file"
    echo "  $0 --command \"STATUS\" --output status.log"
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
        -c|--command)
            COMMAND="$2"
            shift 2
            ;;
        -t|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -i|--interactive)
            INTERACTIVE=1
            shift
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
        exit 1
    fi
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Error: Port does not exist: $PORT"
    exit 1
fi

# Validate interactive vs command mode
if [ $INTERACTIVE -eq 0 ] && [ -z "$COMMAND" ]; then
    echo "❌ Error: Must specify --command or use --interactive mode"
    usage
fi

echo "📡 ESP32 Serial Command Test"
echo "   Port: $PORT"
echo "   Baudrate: $BAUDRATE"
echo "   Timeout: ${TIMEOUT}s"
echo ""

# Clean up any existing processes using the port
fuser -k "$PORT" 2>/dev/null || true
sleep 0.5

# Configure serial port
stty -F "$PORT" "$BAUDRATE" raw -echo -echoe -echok 2>/dev/null || {
    echo "⚠️  Warning: Could not configure port settings"
}

# Function to send command and capture response
send_command() {
    local cmd="$1"
    local temp_file=$(mktemp)
    
    echo "📤 Sending: $cmd"
    
    # Start capturing responses
    timeout "$TIMEOUT" cat "$PORT" > "$temp_file" &
    local cat_pid=$!
    
    # Small delay to ensure cat is ready
    sleep 0.2
    
    # Send command (with newline)
    echo "$cmd" > "$PORT"
    
    # Wait for timeout
    wait $cat_pid 2>/dev/null
    
    # Check if we got any response
    if [ -s "$temp_file" ]; then
        echo "📥 Response:"
        echo "════════════════════════════════════════"
        cat "$temp_file"
        echo "════════════════════════════════════════"
        
        # Save to output file if specified
        if [ -n "$OUTPUT_FILE" ]; then
            echo "=== Command: $cmd ===" >> "$OUTPUT_FILE"
            cat "$temp_file" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        fi
        
        local line_count=$(wc -l < "$temp_file")
        echo "✅ Received $line_count lines"
    else
        echo "⚠️  No response received within ${TIMEOUT}s"
        echo ""
        echo "Troubleshooting:"
        echo "  - Is the ESP32 running and processing serial commands?"
        echo "  - Check baudrate matches ESP32 code (currently: $BAUDRATE)"
        echo "  - Try increasing --timeout value"
        echo "  - Run: ./capture_serial.sh to see all ESP32 output"
    fi
    
    rm -f "$temp_file"
    echo ""
}

# Function for interactive mode
interactive_mode() {
    echo "🔧 Interactive Mode"
    echo "   Type commands and press Enter"
    echo "   Type 'quit' or 'exit' to stop"
    echo "   Press Ctrl+C to abort"
    echo ""
    
    # Start continuous monitor in background
    local monitor_file=$(mktemp)
    cat "$PORT" > "$monitor_file" &
    local monitor_pid=$!
    
    # Cleanup function
    cleanup_interactive() {
        if kill -0 $monitor_pid 2>/dev/null; then
            kill $monitor_pid 2>/dev/null || true
        fi
        rm -f "$monitor_file"
        echo ""
        echo "Interactive session ended"
    }
    trap cleanup_interactive EXIT INT TERM
    
    while true; do
        # Show any incoming data
        if [ -s "$monitor_file" ]; then
            echo "📥 Response:"
            cat "$monitor_file"
            > "$monitor_file"  # Clear file
            echo ""
        fi
        
        # Prompt for command
        read -p "Command> " cmd
        
        # Check for exit commands
        if [ "$cmd" = "quit" ] || [ "$cmd" = "exit" ]; then
            break
        fi
        
        # Skip empty commands
        if [ -z "$cmd" ]; then
            continue
        fi
        
        # Send command
        echo "$cmd" > "$PORT"
        echo "📤 Sent: $cmd"
        
        # Give time for response
        sleep 0.5
    done
}

# Main execution
if [ $INTERACTIVE -eq 1 ]; then
    interactive_mode
else
    send_command "$COMMAND"
fi

exit 0
