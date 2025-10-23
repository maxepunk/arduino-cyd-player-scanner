# Application.h Compilation Fixes Needed

## Summary
The parallel agents made assumptions about service method signatures that don't match the actual implementations from Phases 1-4. This document lists all required fixes.

## Fix Categories

### 1. ConfigService Interface Corrections

**Problem:** Agents assumed `get()` and `load()`, actual methods are `getConfig()` and `loadFromSD()`

**Fixes Required:**
- Line 482: `config.get().teamID` → `config.getConfig().teamID`
- Line 483: `config.get().deviceID` → `config.getConfig().deviceID`
- Line 746: `config.load()` → `config.loadFromSD()`
- Line 753: `auto cfg = config.get()` → `auto cfg = config.getConfig()`
- Line 760: `_debugMode = config.get().debugMode` → `_debugMode = config.getConfig().debugMode`

### 2. OrchestratorService Interface Corrections

**Problem:** Multiple method signature mismatches

**Fixes Required:**
- Line 488: `orchestrator.getConnectionState()` → `orchestrator.getState()`
- Line 491: `orchestrator.sendScan(scan)` → `orchestrator.sendScan(scan, config.getConfig())`
- Line 493: `orchestrator.queueScan(scan)` → Keep as-is (if queueScan exists with 1 param)
- Line 497: `orchestrator.queueScan(scan)` → Keep as-is
- Line 765: `orchestrator.begin()` → `orchestrator.initialize(config.getConfig())`
- Line 768: `orchestrator.isConnected()` → `orchestrator.getState() == models::ORCH_CONNECTED`

### 3. SDCard Interface Corrections

**Problem:** `isAvailable()` doesn't exist, actual method is `isPresent()`

**Fixes Required:**
- Line 737: `sd.isAvailable()` → `sd.isPresent()`

### 4. DisplayDriver Interface Corrections

**Problem:** DisplayDriver is a wrapper, TFT methods need getTFT() accessor

**Fixes Required:**
All direct display method calls need `display.getTFT().method()` instead of `display.method()`:

- Line 681: `display.setTextColor(0xFFE0)` → `display.getTFT().setTextColor(0xFFE0)`
- Line 682: `display.setTextSize(2)` → `display.getTFT().setTextSize(2)`
- Line 683: `display.setCursor(0, 0)` → `display.getTFT().setCursor(0, 0)`
- Line 684-686: `display.println(...)` → `display.getTFT().println(...)`
- Line 704: `display.println(...)` → `display.getTFT().println(...)`
- Line 710-711: Similar fixes
- Line 714-715: Similar fixes
- Line 719-720: Similar fixes
- Line 739-740: Similar fixes
- Line 748-749: Similar fixes
- Line 769-770: Similar fixes
- Line 772: Similar fix

## Comprehensive Sed Script

Due to the large number of fixes, here's a sed script to apply all changes:

```bash
# ConfigService fixes
sed -i 's/config\.get()/config.getConfig()/g' Application.h
sed -i 's/config\.load()/config.loadFromSD()/g' Application.h

# OrchestratorService fixes
sed -i 's/orchestrator\.getConnectionState()/orchestrator.getState()/g' Application.h
sed -i 's/orchestrator\.sendScan(scan)/orchestrator.sendScan(scan, config.getConfig())/g' Application.h
sed -i 's/orchestrator\.begin()/orchestrator.initialize(config.getConfig())/g' Application.h
sed -i 's/orchestrator\.isConnected()/orchestrator.getState() == models::ORCH_CONNECTED/g' Application.h

# SDCard fixes
sed -i 's/sd\.isAvailable()/sd.isPresent()/g' Application.h

# DisplayDriver fixes (more complex - need to preserve method chains)
sed -i 's/display\.setTextColor(/display.getTFT().setTextColor(/g' Application.h
sed -i 's/display\.setTextSize(/display.getTFT().setTextSize(/g' Application.h
sed -i 's/display\.setCursor(/display.getTFT().setCursor(/g' Application.h
sed -i 's/display\.println(/display.getTFT().println(/g' Application.h
```

## Estimated Lines Affected
- **ConfigService:** 5 lines
- **OrchestratorService:** 6 lines
- **SDCard:** 1 line
- **DisplayDriver:** ~16 lines

**Total:** ~28 lines requiring fixes

## Testing After Fixes
1. Compile test-sketches/60-application/
2. Check for remaining errors
3. Verify method signatures match actual service implementations
4. Test on hardware if compilation succeeds
