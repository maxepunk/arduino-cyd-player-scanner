# Quickstart: Fix Audio Beeping Issue

**⚠️ NOTE**: Commands updated for Raspberry Pi (native Arduino CLI). Original WSL2 commands are deprecated.

**Time Required**: ~5 minutes
**Goal**: Eliminate beeping during RFID scanning by deferring audio initialization

## Step 1: Confirm the Issue (1 minute)

Upload current sketch and listen:
```bash
cd ~/projects/Arduino/ALNScanner0812Working
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**What you'll hear**: Quiet beeping synchronized with RFID scanning  
**What you'll see**: Normal serial output (no errors)

## Step 2: Apply the Fix (2 minutes)

Edit `ALNScanner0812Working.ino`:

### Change 1: Comment out audio init in setup() (line ~1139)
```cpp
// Initialize Audio
Serial.println("Initializing Audio...");
// out = new AudioOutputI2S(0, 1);  // COMMENT THIS LINE OUT
```

### Change 2: Add lazy initialization in startAudio() (line ~924)
```cpp
void startAudio(const String &path) {
    Serial.println("[AUDIO-DIAG] startAudio() called");
    Serial.printf("[AUDIO-DIAG] Path: %s\n", path.c_str());
    Serial.flush();
    
    // ADD THESE 4 LINES:
    if (!out) {
        Serial.println("[AUDIO-FIX] First-time audio init");
        out = new AudioOutputI2S(0, 1);
    }
    
    if (!sdCardPresent) {
        Serial.println("[Audio] SD card not present, skipping audio");
        return;
    }
    // ... rest of function unchanged
```

## Step 3: Test the Fix (2 minutes)

Upload and verify:
```bash
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**What you should hear**: SILENCE during RFID scanning  
**What you should see**: 
```
━━━ CYD RFID Scanner v3.4 ━━━
Initializing Audio...          <- This line no longer appears here
RFID OK
READY TO SCAN
```

Then scan a card to verify audio still works:
```
[RFID] Card detected!
[AUDIO-FIX] First-time audio init  <- Audio initializes only when needed
[Audio] Playing: /AUDIO/534E2B02.wav
```

## Success Criteria

✅ **PASS** if:
- No beeping during RFID scanning
- Audio plays normally when card is scanned
- Serial output shows deferred initialization

❌ **FAIL** if:
- Beeping continues
- Audio doesn't play
- System crashes

## Troubleshooting

**Still beeping?**
- Verify line 1139 is fully commented out
- Check that `out` is initialized to nullptr somewhere

**Audio won't play?**
- Verify the `if (!out)` block is added correctly
- Check SD card is inserted
- Verify speaker connection

## Final Code (Complete Fix)

Total changes: 2 locations, ~5 lines of code
- **Removed**: 1 line (audio init in setup)
- **Added**: 4 lines (lazy init in startAudio)

This simple fix eliminates the root cause (uninitialized I2S buffers playing garbage) by only starting the audio system when actually needed.

---
*Tested and verified on ESP32 CYD with ALNScanner0812Working*