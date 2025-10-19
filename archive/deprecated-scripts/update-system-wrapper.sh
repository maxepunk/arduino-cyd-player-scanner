#!/bin/bash
# Update system Arduino CLI wrapper with the final version
# This script safely replaces the system wrapper with our tested version

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Arduino CLI Wrapper Update Tool              ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════╝${NC}"
echo ""

# Check if final wrapper exists
if [ ! -f "arduino-cli-final.sh" ]; then
    echo -e "${RED}Error: arduino-cli-final.sh not found in current directory${NC}"
    exit 1
fi

# Create timestamped backup
BACKUP_NAME="/usr/local/bin/arduino-cli.backup.$(date +%Y%m%d_%H%M%S)"
echo "Creating backup: $BACKUP_NAME"

if [ -w "/usr/local/bin" ]; then
    cp /usr/local/bin/arduino-cli "$BACKUP_NAME"
else
    echo -e "${YELLOW}Note: sudo required for system update${NC}"
    sudo cp /usr/local/bin/arduino-cli "$BACKUP_NAME"
fi

echo -e "${GREEN}✓ Backup created${NC}"

# Update the system wrapper
echo "Installing new wrapper..."
if [ -w "/usr/local/bin" ]; then
    cp arduino-cli-final.sh /usr/local/bin/arduino-cli
    chmod +x /usr/local/bin/arduino-cli
else
    sudo cp arduino-cli-final.sh /usr/local/bin/arduino-cli
    sudo chmod +x /usr/local/bin/arduino-cli
fi

echo -e "${GREEN}✓ Wrapper updated${NC}"

# Test the new wrapper
echo ""
echo "Testing new wrapper..."
if arduino-cli version &>/dev/null; then
    VERSION=$(arduino-cli version | head -1)
    echo -e "${GREEN}✓ Test passed: $VERSION${NC}"
else
    echo -e "${RED}✗ Test failed!${NC}"
    echo "Restoring backup..."
    if [ -w "/usr/local/bin" ]; then
        cp "$BACKUP_NAME" /usr/local/bin/arduino-cli
    else
        sudo cp "$BACKUP_NAME" /usr/local/bin/arduino-cli
    fi
    echo "Backup restored. Please check the wrapper manually."
    exit 1
fi

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Update Complete! 🎉                          ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
echo ""
echo "Changes in this version:"
echo "  • Automatic temp build directory for compilation"
echo "  • UNC path warnings filtered (compile only)"
echo "  • Real-time output preserved for monitor/upload"
echo "  • Exit codes properly propagated"
echo ""
echo "Backup saved as: $BACKUP_NAME"
echo ""
echo "Test compilation with:"
echo "  cd test-sketch"
echo "  arduino-cli compile --fqbn esp32:esp32:esp32 ."