#!/bin/bash
# ESP32 Arduino CLI Setup Script
# Sets up Arduino CLI for ESP32 development on Debian/Ubuntu systems

set -e

echo "🚀 Setting up Arduino CLI for ESP32 development..."

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo "❌ arduino-cli not found. Installing..."
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
    sudo mv bin/arduino-cli /usr/local/bin/
    rm -rf bin/
fi

echo "✅ Arduino CLI found: $(arduino-cli version)"

# Initialize configuration if it doesn't exist
if [ ! -f ~/.arduino15/arduino-cli.yaml ]; then
    echo "📝 Initializing Arduino CLI configuration..."
    arduino-cli config init
fi

# Add ESP32 board manager URL
echo "🔧 Adding ESP32 board manager URL..."
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update board index
echo "📦 Updating package index..."
arduino-cli core update-index

# Install ESP32 core
echo "⬇️  Installing ESP32 core (this may take a few minutes)..."
arduino-cli core install esp32:esp32

# Install commonly used ESP32 libraries
echo "📚 Installing common ESP32 libraries..."
libraries=(
    "Adafruit GFX Library"
    "TFT_eSPI"
    "PubSubClient"
    "ArduinoJson"
    "ESP32Servo"
)

for lib in "${libraries[@]}"; do
    echo "  Installing: $lib"
    arduino-cli lib install "$lib" 2>/dev/null || echo "  ⚠️  Already installed or not found: $lib"
done

# Check for Python (required by ESP32 toolchain)
if ! command -v python3 &> /dev/null; then
    echo "❌ Python3 not found. Installing..."
    sudo apt-get update && sudo apt-get install -y python3 python3-pip
fi

# Check for Python symlink (required by esptool)
if ! command -v python &> /dev/null; then
    echo "🔗 Creating python symlink..."
    sudo apt-get install -y python-is-python3
fi

# Add user to dialout group for serial port access
if ! groups | grep -q dialout; then
    echo "👤 Adding user to dialout group for serial port access..."
    sudo usermod -a -G dialout $USER
    echo "⚠️  You need to log out and log back in for group changes to take effect!"
fi

echo ""
echo "✅ Setup complete!"
echo ""
echo "Installed ESP32 core version:"
arduino-cli core list | grep esp32
echo ""
echo "To verify your setup, run: ./verify_setup.sh"
echo "If you were added to the dialout group, please log out and log back in."
