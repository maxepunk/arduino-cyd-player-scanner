#!/bin/bash
#
# Compile helper for test sketches
# Usage: ./compile-test.sh <sketch-number>
# Example: ./compile-test.sh 38

if [ -z "$1" ]; then
  echo "Usage: $0 <sketch-number>"
  echo "Example: $0 38"
  exit 1
fi

SKETCH_NUM=$1
SKETCH_DIR="test-sketches/${SKETCH_NUM}-*"

# Find the sketch directory
SKETCH_PATH=$(find test-sketches -maxdepth 1 -type d -name "${SKETCH_NUM}-*" | head -1)

if [ -z "$SKETCH_PATH" ]; then
  echo "Error: Sketch ${SKETCH_NUM} not found"
  exit 1
fi

echo "Compiling ${SKETCH_PATH}..."

arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/MFRC522 \
  --library ~/projects/Arduino/libraries/ESP8266Audio \
  --library ~/projects/Arduino/libraries/XPT2046_Touchscreen \
  "${SKETCH_PATH}"
