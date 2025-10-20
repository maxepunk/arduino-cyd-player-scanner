#!/bin/bash
# ESP32 Compilation Script
# Compiles ESP32 sketches with common configuration options

# Default values
FQBN="esp32:esp32:esp32"
SKETCH_PATH=""
VERBOSE=""
EXPORT_BINARIES=""
BUILD_PROPERTIES=""

# Common FQBN configurations
declare -A BOARD_CONFIGS
BOARD_CONFIGS["default"]="esp32:esp32:esp32"
BOARD_CONFIGS["esp32-wrover"]="esp32:esp32:esp32wrover"
BOARD_CONFIGS["esp32-s2"]="esp32:esp32:esp32s2"
BOARD_CONFIGS["esp32-s3"]="esp32:esp32:esp32s3"
BOARD_CONFIGS["esp32-c3"]="esp32:esp32:esp32c3"

# Performance configurations
BOARD_CONFIGS["fast"]="esp32:esp32:esp32:CPUFreq=240,FlashMode=qio,FlashFreq=80"
BOARD_CONFIGS["balanced"]="esp32:esp32:esp32:CPUFreq=160,FlashMode=qio,FlashFreq=80"
BOARD_CONFIGS["lowpower"]="esp32:esp32:esp32:CPUFreq=80,FlashMode=dio,FlashFreq=40"

# Partition schemes
BOARD_CONFIGS["huge_app"]="esp32:esp32:esp32:PartitionScheme=huge_app"
BOARD_CONFIGS["min_spiffs"]="esp32:esp32:esp32:PartitionScheme=min_spiffs"
BOARD_CONFIGS["no_ota"]="esp32:esp32:esp32:PartitionScheme=no_ota"

usage() {
    echo "Usage: $0 [OPTIONS] <sketch_path>"
    echo ""
    echo "Options:"
    echo "  -b, --board BOARD        Board configuration (default: default)"
    echo "  -f, --fqbn FQBN          Full FQBN string (overrides --board)"
    echo "  -v, --verbose            Enable verbose compilation output"
    echo "  -e, --export-binaries    Export compiled binaries"
    echo "  -p, --property KEY=VALUE Add build property"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Available board configurations:"
    for key in "${!BOARD_CONFIGS[@]}"; do
        echo "  $key: ${BOARD_CONFIGS[$key]}"
    done
    echo ""
    echo "Examples:"
    echo "  $0 my_sketch.ino"
    echo "  $0 --board fast --verbose my_sketch.ino"
    echo "  $0 --fqbn esp32:esp32:esp32:CPUFreq=240 my_sketch.ino"
    echo "  $0 --property compiler.cpp.extra_flags=-DDEBUG_MODE my_sketch.ino"
    exit 1
}

# Parse arguments
BOARD_CONFIG="default"
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--board)
            BOARD_CONFIG="$2"
            shift 2
            ;;
        -f|--fqbn)
            FQBN="$2"
            BOARD_CONFIG=""
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="--verbose"
            shift
            ;;
        -e|--export-binaries)
            EXPORT_BINARIES="--export-binaries"
            shift
            ;;
        -p|--property)
            BUILD_PROPERTIES="$BUILD_PROPERTIES --build-property $2"
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

if [ ! -f "$SKETCH_PATH" ] && [ ! -d "$SKETCH_PATH" ]; then
    echo "❌ Error: Sketch not found: $SKETCH_PATH"
    exit 1
fi

# Use board configuration if FQBN not explicitly set
if [ -n "$BOARD_CONFIG" ]; then
    if [ -n "${BOARD_CONFIGS[$BOARD_CONFIG]}" ]; then
        FQBN="${BOARD_CONFIGS[$BOARD_CONFIG]}"
    else
        echo "❌ Error: Unknown board configuration: $BOARD_CONFIG"
        echo "Available configurations: ${!BOARD_CONFIGS[@]}"
        exit 1
    fi
fi

echo "🔨 Compiling ESP32 sketch..."
echo "   Sketch: $SKETCH_PATH"
echo "   FQBN: $FQBN"

# Build the compile command
CMD="arduino-cli compile --fqbn $FQBN $VERBOSE $EXPORT_BINARIES $BUILD_PROPERTIES $SKETCH_PATH"

echo "   Command: $CMD"
echo ""

# Execute compilation
eval $CMD

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilation successful!"
    
    # Show binary info if exported
    if [ -n "$EXPORT_BINARIES" ]; then
        SKETCH_DIR=$(dirname "$SKETCH_PATH")
        SKETCH_NAME=$(basename "$SKETCH_PATH" .ino)
        BIN_FILE="$SKETCH_DIR/build/esp32.esp32.esp32/$SKETCH_NAME.ino.bin"
        if [ -f "$BIN_FILE" ]; then
            SIZE=$(du -h "$BIN_FILE" | cut -f1)
            echo "   Binary size: $SIZE"
            echo "   Location: $BIN_FILE"
        fi
    fi
else
    echo ""
    echo "❌ Compilation failed!"
    exit 1
fi
