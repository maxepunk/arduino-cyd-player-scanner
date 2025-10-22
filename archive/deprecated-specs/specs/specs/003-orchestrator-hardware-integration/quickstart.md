# Quickstart Guide: Scanner Setup for Game Masters

**Feature**: 003-orchestrator-hardware-integration
**Audience**: Game Masters (non-technical users)
**Date**: 2025-10-19

## Overview

This guide helps Game Masters prepare scanners for gameplay by configuring WiFi and orchestrator settings.

**Time Required**: 5 minutes per scanner

**What You'll Need**:
- Scanner with SD card slot
- Computer with SD card reader (built-in or USB adapter)
- Text editor (Notepad, TextEdit, VS Code, etc.)
- WiFi network name and password
- Orchestrator server IP address

---

## Step-by-Step Setup

### Step 1: Remove SD Card from Scanner

1. **Power off scanner** (if currently on)
2. **Locate SD card slot** on back of scanner
3. **Gently push SD card** inward to release
4. **SD card will pop out** slightly
5. **Pull SD card out** carefully

**Safety**: Always power off scanner before removing SD card to prevent data corruption.

---

### Step 2: Insert SD Card into Computer

1. **Locate SD card reader** on your computer:
   - **Laptop**: Built-in SD card slot on side
   - **Desktop**: USB SD card reader adapter
2. **Insert SD card** into reader slot
3. **Wait for computer to recognize** SD card:
   - **Windows**: Drive letter appears (e.g., "E:")
   - **Mac**: Icon appears on desktop (e.g., "NO NAME")
   - **Linux**: Mounted at `/media/` or `/mnt/`

**Troubleshooting**: If SD card doesn't appear:
- Try reinserting SD card
- Check SD card orientation (metal contacts facing down/correct direction)
- Try different USB port (if using adapter)

---

### Step 3: Open Configuration File

1. **Navigate to SD card** in file explorer:
   - **Windows**: Open "This PC" → Click drive letter (e.g., "E:")
   - **Mac**: Double-click SD card icon on desktop
   - **Linux**: Open file manager → Click SD card under "Devices"

2. **Look for file named `config.txt`** in root of SD card
   - If file exists: Double-click to open
   - If file doesn't exist: Create new file (see "Creating Config File" below)

3. **Open with text editor**:
   - **Windows**: Notepad (right-click → "Open with" → "Notepad")
   - **Mac**: TextEdit (right-click → "Open With" → "TextEdit", ensure "Plain Text" mode)
   - **Any OS**: VS Code, Sublime Text, Atom (if installed)

---

### Step 4: Edit Configuration Values

**Template** (copy and paste, then edit values):

```
WIFI_SSID=VenueNetworkName
WIFI_PASSWORD=wifipassword123
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
DEVICE_ID=SCANNER_01
```

**Fields to Edit**:

#### WIFI_SSID

- **What it is**: Your venue's WiFi network name
- **How to get it**: Ask venue tech support or check venue WiFi settings
- **Example**: `WIFI_SSID=AboutLastNightVenue`
- **Rules**: 1-32 characters, exact match (case-sensitive)

#### WIFI_PASSWORD

- **What it is**: Your venue's WiFi password
- **How to get it**: Ask venue tech support
- **Example**: `WIFI_PASSWORD=gamenight2025`
- **Rules**: 0-63 characters (leave empty for open WiFi: `WIFI_PASSWORD=`)

#### ORCHESTRATOR_URL

- **What it is**: IP address and port of orchestrator server
- **How to get it**: Ask technical team running orchestrator server
- **Format**: `http://IP_ADDRESS:PORT`
- **Example**: `ORCHESTRATOR_URL=http://10.0.0.100:3000`
- **Rules**:
  - Must start with `http://` (NOT `https://`)
  - No trailing slash (WRONG: `http://10.0.0.100:3000/`)
  - Include port number (usually `:3000`)

#### TEAM_ID

- **What it is**: Team number for this scanner
- **How to decide**: Assign team numbers 001, 002, 003, etc. for each team
- **Example**: `TEAM_ID=001` (Team 1), `TEAM_ID=002` (Team 2)
- **Rules**: Exactly 3 digits (use leading zeros: `001`, not `1`)

#### DEVICE_ID (Optional)

- **What it is**: Custom name for this specific scanner
- **When to use**: If you want descriptive names instead of auto-generated IDs
- **Example**: `DEVICE_ID=SCANNER_ALPHA`, `DEVICE_ID=TEAM1_SCANNER`
- **Rules**: Letters, numbers, and underscores only (no spaces)
- **If omitted**: Scanner generates ID from MAC address (e.g., `SCANNER_A1B2C3D4E5F6`)

---

### Step 5: Save Configuration File

1. **Save file** in text editor:
   - **Keyboard shortcut**: Ctrl+S (Windows/Linux) or Cmd+S (Mac)
   - **Menu**: File → Save

2. **Important**: Ensure file is named exactly `config.txt` (all lowercase)
   - **Windows users**: Show file extensions to avoid `config.txt.txt`
     - Open File Explorer → Click "View" tab → Check "File name extensions"

3. **Close text editor**

---

### Step 6: Safely Eject SD Card

**Why this matters**: Prevents data corruption by ensuring all writes complete.

**How to eject**:

- **Windows**:
  1. Click "Safely Remove Hardware" icon in system tray
  2. Click "Eject [SD card name]"
  3. Wait for "Safe to remove hardware" message

- **Mac**:
  1. Right-click SD card icon on desktop
  2. Click "Eject"
  3. Wait for icon to disappear

- **Linux**:
  1. Right-click SD card in file manager
  2. Click "Unmount" or "Eject"
  3. Wait for confirmation

4. **Remove SD card** from computer

---

### Step 7: Insert SD Card Back into Scanner

1. **Orient SD card correctly** (metal contacts facing correct direction)
2. **Gently push SD card** into slot until it clicks
3. **Power on scanner** (press power button or plug in)

---

### Step 8: Verify Configuration

**What to look for on scanner display**:

#### Success Indicators

- **"Connecting..."**: Scanner is attempting WiFi connection (wait up to 30 seconds)
- **"Connected ✓"**: Scanner is online and connected to orchestrator (success!)
- **"Ready - Offline"**: WiFi connected, but orchestrator unreachable (see troubleshooting)

#### Error Messages

- **"WiFi Error - Check Config"**: WiFi connection failed
  - Check WIFI_SSID and WIFI_PASSWORD are correct
  - Verify WiFi network is active
  - Scanner will retry every 30 seconds

- **"Config Error: Missing [FIELD]"**: Required field missing from config.txt
  - Check all required fields present (WIFI_SSID, WIFI_PASSWORD, ORCHESTRATOR_URL, TEAM_ID)
  - Verify no typos in field names (case-sensitive)

- **"Config Error: Invalid [FIELD]"**: Field value doesn't match format rules
  - Check TEAM_ID is exactly 3 digits (e.g., `001`)
  - Check ORCHESTRATOR_URL starts with `http://`
  - Check WiFi SSID is 1-32 characters

- **"Config Error: No config.txt"**: File not found
  - Verify file named exactly `config.txt` (not `Config.txt` or `config.txt.txt`)
  - Verify file is in root of SD card (not in subfolder)

---

## Troubleshooting

### Scanner Says "WiFi Error - Check Config"

**Possible Causes**:
1. WiFi SSID or password incorrect
2. WiFi network not broadcasting (hidden SSID)
3. WiFi network out of range
4. WiFi network using unsupported security (WPA/WPA2 required)

**Solutions**:
- Verify WiFi credentials with venue tech support
- Move scanner closer to WiFi router
- Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Try different WiFi network

---

### Scanner Says "Ready - Offline"

**What this means**: WiFi connected, but orchestrator server unreachable

**Possible Causes**:
1. Orchestrator URL incorrect (wrong IP address or port)
2. Orchestrator server not running
3. Network firewall blocking connection

**Solutions**:
- Verify ORCHESTRATOR_URL with technical team
- Check orchestrator server is running (ask tech team)
- Verify orchestrator port 3000 is accessible on network
- Test from another device: Open browser, go to `http://ORCHESTRATOR_IP:3000/health`
  - Should see: `{"status":"online",...}`

**Note**: Scanner will continue working in offline mode (scans are queued for later upload).

---

### Scanner Says "Config Error: Invalid TEAM_ID"

**Common Mistake**: Using 1 or 2 digits instead of 3

**Fix**:
- WRONG: `TEAM_ID=1`
- CORRECT: `TEAM_ID=001`

**Rule**: Always use exactly 3 digits with leading zeros (001, 002, ..., 999)

---

### Scanner Says "Config Error: Invalid ORCHESTRATOR_URL"

**Common Mistakes**:
1. Using HTTPS instead of HTTP
2. Adding trailing slash
3. Missing `http://` prefix
4. Including spaces

**Examples**:

| Wrong | Correct |
|-------|---------|
| `https://10.0.0.100:3000` | `http://10.0.0.100:3000` |
| `http://10.0.0.100:3000/` | `http://10.0.0.100:3000` |
| `10.0.0.100:3000` | `http://10.0.0.100:3000` |
| `http:// 10.0.0.100:3000` | `http://10.0.0.100:3000` |

---

### File Named `config.txt.txt` (Windows)

**Problem**: Windows hides file extensions by default

**Solution**:
1. Open File Explorer
2. Click "View" tab
3. Check "File name extensions"
4. Rename file to `config.txt` (remove duplicate `.txt`)

---

### Can't Create config.txt (Mac TextEdit)

**Problem**: TextEdit defaults to Rich Text Format (.rtf)

**Solution**:
1. Open TextEdit
2. Menu: Format → Make Plain Text (Cmd+Shift+T)
3. Type configuration
4. Save as `config.txt`

---

### SD Card Won't Eject

**Problem**: Computer still using SD card files

**Solution**:
1. Close all programs accessing SD card (text editor, file explorer)
2. Wait 5 seconds
3. Try ejecting again
4. If still stuck: Restart computer, then eject

---

## Advanced: Multiple Scanners Setup

### Using Same Config for All Scanners

**If all scanners on same team**:
1. Create `config.txt` on one SD card
2. Eject SD card
3. Insert into computer
4. Copy `config.txt` to desktop
5. Insert next SD card
6. Paste `config.txt` to new SD card
7. Repeat for all scanners

**Result**: All scanners use same TEAM_ID, auto-generated unique DEVICE_IDs

---

### Using Different Teams

**If assigning different teams**:
1. Create `config.txt` on first SD card with `TEAM_ID=001`
2. Copy to desktop
3. Insert next SD card
4. Paste `config.txt`
5. **Edit TEAM_ID** to `002`
6. Save
7. Repeat, incrementing TEAM_ID (003, 004, etc.)

**Result**: Each scanner assigned to different team

---

### Using Custom Device IDs

**If you want descriptive scanner names**:
1. Create `config.txt` with `DEVICE_ID=SCANNER_ALPHA`
2. For next scanner, edit to `DEVICE_ID=SCANNER_BETA`
3. Repeat with unique names: `TEAM1_SCANNER`, `TEAM2_SCANNER`, etc.

**Result**: Scanner status shows descriptive names instead of MAC addresses

---

## Testing Configuration

### Test Checklist

After setup, verify:

- [ ] Scanner display shows "Connected ✓" or "Ready - Offline"
- [ ] No error messages on display
- [ ] Scanner beeps when RFID card scanned (RFID working)
- [ ] Scanner displays image/audio for non-video tokens (local media working)
- [ ] Scanner shows "Sending..." for video tokens (network working)

### View Scanner Status

**Tap display once** during idle mode to see status screen:

- WiFi SSID: VenueNetwork ✓
- Orchestrator: Connected ✓
- Queue: 0 scans
- Team: 001
- Device: SCANNER_A1B2C3D4E5F6

**Tap again** to dismiss status screen

---

## Common Configuration Examples

### Example 1: Basic Setup (Team 1)

```
WIFI_SSID=AboutLastNightVenue
WIFI_PASSWORD=gamenight2025
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
```

---

### Example 2: Multiple Teams

**Team 1 Scanner**:
```
WIFI_SSID=VenueWiFi
WIFI_PASSWORD=password123
ORCHESTRATOR_URL=http://192.168.1.50:3000
TEAM_ID=001
DEVICE_ID=TEAM1_SCANNER
```

**Team 2 Scanner**:
```
WIFI_SSID=VenueWiFi
WIFI_PASSWORD=password123
ORCHESTRATOR_URL=http://192.168.1.50:3000
TEAM_ID=002
DEVICE_ID=TEAM2_SCANNER
```

---

### Example 3: Open WiFi Network

```
WIFI_SSID=PublicVenueWiFi
WIFI_PASSWORD=
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=003
```

**Note**: Empty password for open WiFi networks

---

## Pre-Game Checklist

### Before Players Arrive

- [ ] All scanners configured with correct WiFi credentials
- [ ] All scanners assigned to correct teams (TEAM_ID)
- [ ] All scanners showing "Connected ✓" or "Ready - Offline"
- [ ] All scanners tested with RFID card scan
- [ ] Orchestrator server running and reachable
- [ ] Venue WiFi network stable and operational
- [ ] SD cards contain latest token database and media files

### During Game

- [ ] Monitor scanner status screens for connection issues
- [ ] Check orchestrator for incoming scans (verify scanners uploading)
- [ ] If scanner shows "Queue: N scans", verify connection restored eventually

### After Game

- [ ] Check orchestrator logs for all scans received
- [ ] Download scanner queue files if any scans still queued
- [ ] Note any network issues for venue tech team

---

## Getting Help

### During Setup

**If you encounter errors not covered in this guide**:
1. Write down exact error message from scanner display
2. Take photo of `config.txt` file (check for typos)
3. Contact technical support team

### Technical Support Information

**Include in support request**:
- Scanner error message (from display)
- Contents of `config.txt` file (hide password if sharing publicly)
- WiFi network type (open, WPA2)
- Orchestrator server IP address and port

---

## Appendix: File Format Reference

### Valid config.txt Format

```
WIFI_SSID=NetworkName
WIFI_PASSWORD=password123
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
DEVICE_ID=SCANNER_01
```

### Rules

- One setting per line
- Format: `KEY=VALUE`
- No spaces around `=` (though scanner trims spaces, best to avoid)
- No blank lines (scanner ignores them, but best to avoid)
- No comments (scanner skips lines without `=`, but don't rely on this)
- File must be named exactly `config.txt` (lowercase)

### Required Fields

- `WIFI_SSID` (1-32 characters)
- `WIFI_PASSWORD` (0-63 characters, empty for open WiFi)
- `ORCHESTRATOR_URL` (must start with `http://`, no trailing slash)
- `TEAM_ID` (exactly 3 digits: 000-999)

### Optional Fields

- `DEVICE_ID` (1-100 characters, letters/numbers/underscores only)

---

**Quickstart Guide Complete**: 2025-10-19
**Version**: 1.0

*For technical implementation details, see `spec.md`, `data-model.md`, and `contracts/` directory.*
