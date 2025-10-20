#!/bin/bash
# ESP32 Setup Verification Script
# Verifies Arduino CLI and ESP32 toolchain installation

echo "🔍 Verifying ESP32 Arduino CLI setup..."
echo ""

ERRORS=0
WARNINGS=0

# Check Arduino CLI
echo "1. Checking Arduino CLI..."
if command -v arduino-cli &> /dev/null; then
    VERSION=$(arduino-cli version)
    echo "   ✅ Arduino CLI found: $VERSION"
else
    echo "   ❌ Arduino CLI not found"
    echo "      Install with: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    ((ERRORS++))
fi
echo ""

# Check ESP32 core
echo "2. Checking ESP32 core..."
if arduino-cli core list | grep -q "esp32:esp32"; then
    CORE_VERSION=$(arduino-cli core list | grep "esp32:esp32" | awk '{print $2}')
    echo "   ✅ ESP32 core installed: $CORE_VERSION"
else
    echo "   ❌ ESP32 core not installed"
    echo "      Install with: arduino-cli core install esp32:esp32"
    ((ERRORS++))
fi
echo ""

# Check Python
echo "3. Checking Python installation..."
if command -v python3 &> /dev/null; then
    PY_VERSION=$(python3 --version)
    echo "   ✅ Python3 found: $PY_VERSION"
else
    echo "   ❌ Python3 not found"
    echo "      Install with: sudo apt-get install python3"
    ((ERRORS++))
fi

if command -v python &> /dev/null; then
    echo "   ✅ Python symlink exists"
else
    echo "   ⚠️  Python symlink not found (esptool.py may fail)"
    echo "      Create with: sudo apt-get install python-is-python3"
    ((WARNINGS++))
fi
echo ""

# Check serial port access
echo "4. Checking serial port permissions..."
if groups | grep -q dialout; then
    echo "   ✅ User is in dialout group"
else
    echo "   ⚠️  User not in dialout group"
    echo "      Add with: sudo usermod -a -G dialout $USER"
    echo "      Then log out and log back in"
    ((WARNINGS++))
fi

# Check for available serial ports
if ls /dev/ttyUSB* 2>/dev/null | grep -q .; then
    echo "   ✅ USB serial ports found:"
    ls -la /dev/ttyUSB* 2>/dev/null | awk '{print "      " $0}'
elif ls /dev/ttyACM* 2>/dev/null | grep -q .; then
    echo "   ✅ ACM serial ports found:"
    ls -la /dev/ttyACM* 2>/dev/null | awk '{print "      " $0}'
else
    echo "   ⚠️  No serial ports detected (is ESP32 connected?)"
    ((WARNINGS++))
fi
echo ""

# Check common libraries
echo "5. Checking common ESP32 libraries..."
LIBRARIES=("Adafruit GFX Library" "TFT_eSPI" "PubSubClient" "ArduinoJson")
MISSING_LIBS=()

for lib in "${LIBRARIES[@]}"; do
    if arduino-cli lib list 2>/dev/null | grep -q "$lib"; then
        echo "   ✅ $lib"
    else
        echo "   ⚠️  $lib not installed"
        MISSING_LIBS+=("$lib")
        ((WARNINGS++))
    fi
done

if [ ${#MISSING_LIBS[@]} -gt 0 ]; then
    echo ""
    echo "   Install missing libraries with:"
    for lib in "${MISSING_LIBS[@]}"; do
        echo "      arduino-cli lib install \"$lib\""
    done
fi
echo ""

# Check board manager configuration
echo "6. Checking board manager configuration..."
if [ -f ~/.arduino15/arduino-cli.yaml ]; then
    if grep -q "package_esp32_index.json" ~/.arduino15/arduino-cli.yaml; then
        echo "   ✅ ESP32 board manager URL configured"
    else
        echo "   ⚠️  ESP32 board manager URL not found"
        echo "      Add with: arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
        ((WARNINGS++))
    fi
else
    echo "   ⚠️  Configuration file not found"
    echo "      Initialize with: arduino-cli config init"
    ((WARNINGS++))
fi
echo ""

# Summary
echo "════════════════════════════════════════════════════════════════"
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo "✅ Setup verification complete - all checks passed!"
    echo ""
    echo "You're ready to start developing ESP32 projects!"
    echo ""
    echo "Quick start:"
    echo "  1. Create a sketch: arduino-cli sketch new MyProject"
    echo "  2. Compile: ./compile_esp32.sh MyProject/MyProject.ino"
    echo "  3. Upload: ./upload_esp32.sh MyProject/MyProject.ino"
    echo "  4. Monitor: ./monitor_serial.sh"
elif [ $ERRORS -eq 0 ]; then
    echo "⚠️  Setup verification complete with $WARNINGS warning(s)"
    echo ""
    echo "Your setup should work, but some optional features may be missing."
else
    echo "❌ Setup verification failed with $ERRORS error(s) and $WARNINGS warning(s)"
    echo ""
    echo "Please address the errors above before continuing."
    exit 1
fi
