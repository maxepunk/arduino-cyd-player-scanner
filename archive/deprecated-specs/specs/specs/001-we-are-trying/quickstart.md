# Quickstart: ALNScanner Software SPI Troubleshooting

## Current Status
- **ILI9341 variant**: Previously worked with different config (untested in current setup)
- **ST7789 variant**: Fails with complete serial loss and non-responsive RFID

## The Real Problem
NOT a display driver issue! The software SPI implementation's critical sections are blocking interrupts too long, causing UART buffer overflow.

## Diagnostic Test Sequence

### 1. Run Test 11: SPI Protocol Verification
```bash
cd ~/projects/Arduino/test-sketches/11-spi-protocol-verify
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```
**Expected**: GPIO3 manipulation tests, SPI Mode 0 verification

### 2. Run Test 12: MFRC522 Version Register
```bash
cd ../12-mfrc522-version
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```
**Expected**: Version register returns 0x91 or 0x92 consistently

### 3. Run Test 13: Critical Section Timing
```bash
cd ../13-critical-timing
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```
**Expected**: Find maximum safe critical section duration before serial fails

## Potential Solutions (Based on Test Results)

### If Critical Sections are the Issue:
```cpp
// Replace long critical section with minimal ones
byte softSPI_transfer(byte data) {
    byte result = 0;
    for(int i = 0; i < 8; ++i) {
        // Only critical for pin changes, not delays
        portENTER_CRITICAL(&spiMux);
        digitalWrite(SOFT_SPI_MOSI, (data & 0x80) ? HIGH : LOW);
        digitalWrite(SOFT_SPI_SCK, HIGH);
        portEXIT_CRITICAL(&spiMux);
        
        delayMicroseconds(2);
        result |= digitalRead(SOFT_SPI_MISO) << (7-i);
        
        portENTER_CRITICAL(&spiMux);
        digitalWrite(SOFT_SPI_SCK, LOW);
        portEXIT_CRITICAL(&spiMux);
        
        data <<= 1;
    }
    return result;
}
```

### If GPIO3/RX Conflict:
- Change RFID_SS from GPIO3 to GPIO4 (requires wiring change)

### If SPI Protocol is Wrong:
- Verify clock polarity (CPOL=0) and phase (CPHA=0)
- Adjust timing to match MFRC522 datasheet

## Total Time to Diagnose: ~30 minutes

Run tests systematically to identify root cause, then apply targeted fix.