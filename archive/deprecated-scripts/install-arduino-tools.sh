#!/bin/bash
# Safe installer for Arduino CLI WSL2 integration tools
# This script installs with proper error checking and backup

set -e  # Exit on any error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Arduino CLI WSL2 Integration Tool Installer  ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════╝${NC}"
echo ""

# Check if running with sudo when needed
INSTALL_DIR="/usr/local/bin"
NEED_SUDO=""
if [ ! -w "$INSTALL_DIR" ]; then
    NEED_SUDO="sudo"
    echo -e "${YELLOW}Note: Installation to $INSTALL_DIR requires sudo${NC}"
fi

# Verify source files exist
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo "Checking source files in: $SCRIPT_DIR"

REQUIRED_FILES=(
    "arduino-cli-wrapper.sh"
    "arduino-upload.sh"
    "arduino-monitor.sh"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$SCRIPT_DIR/$file" ]; then
        echo -e "${RED}Error: Missing required file: $file${NC}"
        exit 1
    fi
    echo -e "  ✓ Found: $file"
done

# Test Arduino CLI access
echo ""
echo "Testing Arduino CLI access..."
if /mnt/c/arduino-cli/arduino-cli.exe version &>/dev/null; then
    echo -e "${GREEN}  ✓ Arduino CLI is accessible${NC}"
else
    echo -e "${RED}Error: Cannot access Arduino CLI at C:\\arduino-cli\\${NC}"
    echo "Please ensure Arduino CLI is installed on Windows"
    exit 1
fi

# Create backups if files already exist
echo ""
echo "Checking for existing installations..."
BACKUP_MADE=false
for file in arduino-cli arduino-upload arduino-monitor; do
    if [ -f "$INSTALL_DIR/$file" ]; then
        BACKUP_NAME="$INSTALL_DIR/${file}.backup.$(date +%Y%m%d_%H%M%S)"
        echo -e "${YELLOW}  Backing up existing $file to $(basename $BACKUP_NAME)${NC}"
        $NEED_SUDO cp "$INSTALL_DIR/$file" "$BACKUP_NAME"
        BACKUP_MADE=true
    fi
done

# Make scripts executable
echo ""
echo "Preparing scripts..."
chmod +x "$SCRIPT_DIR"/*.sh
echo -e "${GREEN}  ✓ Scripts marked executable${NC}"

# Install scripts
echo ""
echo "Installing scripts to $INSTALL_DIR..."

# Install with proper names
$NEED_SUDO cp "$SCRIPT_DIR/arduino-cli-wrapper.sh" "$INSTALL_DIR/arduino-cli"
$NEED_SUDO cp "$SCRIPT_DIR/arduino-upload.sh" "$INSTALL_DIR/arduino-upload"
$NEED_SUDO cp "$SCRIPT_DIR/arduino-monitor.sh" "$INSTALL_DIR/arduino-monitor"

# Ensure executable
$NEED_SUDO chmod +x "$INSTALL_DIR/arduino-cli"
$NEED_SUDO chmod +x "$INSTALL_DIR/arduino-upload"
$NEED_SUDO chmod +x "$INSTALL_DIR/arduino-monitor"

echo -e "${GREEN}  ✓ Scripts installed successfully${NC}"

# Verify installation
echo ""
echo "Verifying installation..."
FAILED=false

if command -v arduino-cli &>/dev/null; then
    echo -e "${GREEN}  ✓ arduino-cli command available${NC}"
else
    echo -e "${RED}  ✗ arduino-cli command not found${NC}"
    FAILED=true
fi

if command -v arduino-upload &>/dev/null; then
    echo -e "${GREEN}  ✓ arduino-upload command available${NC}"
else
    echo -e "${RED}  ✗ arduino-upload command not found${NC}"
    FAILED=true
fi

if command -v arduino-monitor &>/dev/null; then
    echo -e "${GREEN}  ✓ arduino-monitor command available${NC}"
else
    echo -e "${RED}  ✗ arduino-monitor command not found${NC}"
    FAILED=true
fi

if [ "$FAILED" = true ]; then
    echo ""
    echo -e "${YELLOW}Warning: Some commands are not in PATH${NC}"
    echo "You may need to refresh your shell:"
    echo "  source ~/.bashrc"
    echo "Or open a new terminal"
fi

# Test functionality
echo ""
echo "Testing functionality..."
if arduino-cli version &>/dev/null; then
    VERSION=$(arduino-cli version | head -1)
    echo -e "${GREEN}  ✓ Arduino CLI working: $VERSION${NC}"
else
    echo -e "${RED}  ✗ Arduino CLI test failed${NC}"
    exit 1
fi

# Success message
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║         Installation Complete! 🎉              ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
echo ""
echo "Available commands:"
echo "  arduino-cli     - Run any Arduino CLI command"
echo "  arduino-upload  - Compile and upload to board"
echo "  arduino-monitor - Monitor serial output"
echo ""
echo "Quick test:"
echo "  arduino-cli board list"
echo ""

if [ "$BACKUP_MADE" = true ]; then
    echo -e "${YELLOW}Note: Previous versions backed up with .backup extension${NC}"
fi