# Serial Debug Interface

**Simple serial output for beeping diagnosis**  
**Baud Rate**: 115200  
**Format**: Plain text with timestamps

## Debug Output Format

All debug messages follow this simple pattern:
```
[TAG] Message at TIMESTAMP ms
```

## Tags Used
- `[AUDIO]` - Audio system events
- `[POLL]` - RFID polling markers  
- `[CARD]` - Card detection events
- `[PLAY]` - Audio playback events
- `[FIX]` - Fix application status

## Example Session

```
━━━ CYD RFID Scanner v3.4 ━━━
[AUDIO] Init at 3145 ms        <- Problem: Init in setup()
RFID OK
READY TO SCAN
[POLL] 3245 ms
[POLL] 3345 ms                 <- User hears beeping here
[POLL] 3445 ms
[CARD] Detected at 8932 ms
[PLAY] Audio start at 8950 ms  <- Beeping stops
```

## With Fix Applied

```
━━━ CYD RFID Scanner v3.4 ━━━
[FIX] Audio init deferred
RFID OK  
READY TO SCAN
[POLL] 3245 ms                 <- No beeping!
[POLL] 3345 ms
[CARD] Detected at 8932 ms
[AUDIO] Init at 8949 ms        <- Init only when needed
[PLAY] Audio start at 8950 ms
```

No complex commands needed. Just observe the output to verify the fix works.

---
*Simple serial debugging - no complex protocols*