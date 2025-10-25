# SerialService Extraction Notes

## Source Material
- **Original File:** ALNScanner1021_Orchestrator.ino (v4.1)
- **Source Lines:** 2927-3394 (468 lines total)
- **Function:** processSerialCommands()
- **Pattern:** if/else chain with 14+ commands

## Commands Extracted from v4.1

### Core Commands
1. CONFIG - Show configuration
2. STATUS / DIAG_NETWORK - Show connection status
3. TOKENS - Display token database
4. HELP - Show available commands

### Testing Commands
5. TEST_VIDEO:id - Test video token detection
6. TEST_PROC_IMG:path - Test processing image display
7. SIMULATE_SCAN:id - Simulate RFID scan

### Queue Management
8. QUEUE_TEST - Add mock scans
9. FORCE_UPLOAD - Trigger batch upload
10. SHOW_QUEUE - Display queue contents
11. FORCE_OVERFLOW - Test overflow handling

### Configuration Management
12. SET_CONFIG:KEY=VALUE - Update config in memory
13. SAVE_CONFIG - Write config to SD
14. REBOOT - Restart ESP32

### Scanner Control
15. START_SCANNER - Initialize RFID (kills serial RX)

## Architecture Changes

### v4.1 (Monolithic)
```cpp
void processSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        if (cmd == "CONFIG") {
            // 10 lines of implementation
        } else if (cmd == "STATUS") {
            // 18 lines of implementation
        } else if (cmd == "TOKENS") {
            // 12 lines of implementation
        }
        // ... 12 more commands
    }
}
```

**Problems:**
- 468 lines in single function
- if/else chain (O(n) lookup)
- Tightly coupled to global state
- Hard to extend or test
- All logic in main sketch

### v5.0 (Service Layer)
```cpp
// Infrastructure (SerialService.h)
class SerialService {
    using CommandHandler = std::function<void(const String& args)>;
    void registerCommand(const String& name, CommandHandler handler);
    std::vector<Command> _commands; // O(n) lookup, but extensible
};

// Application (Application.cpp)
void Application::registerSerialCommands() {
    _serial.registerCommand("CONFIG", [this](const String& args) {
        _config.print();
    });
    
    _serial.registerCommand("STATUS", [this](const String& args) {
        _orchestrator.printStatus();
    });
    // ... register all other commands
}
```

**Benefits:**
- Separation of concerns (infrastructure vs. implementation)
- Command registry pattern (extensible)
- Dependency injection (testable)
- Commands can be added at runtime
- Each command is a lambda (captures dependencies)

## Implementation Highlights

### Command Registry Pattern
```cpp
struct Command {
    String name;
    CommandHandler handler; // std::function<void(const String&)>
    String help;
};

std::vector<Command> _commands;
```

### Non-blocking Processing
```cpp
void processCommands() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        handleCommand(input);
    }
    // Returns immediately if no data
}
```

### Flexible Argument Parsing
```cpp
void handleCommand(const String& input) {
    String cmd, args;
    
    // Colon-separated: "SET:KEY=VALUE"
    if (input.indexOf(':') != -1) {
        cmd = input.substring(0, input.indexOf(':'));
        args = input.substring(input.indexOf(':') + 1);
    }
    // Space-separated: "ADD 5 3"
    else if (input.indexOf(' ') != -1) {
        cmd = input.substring(0, input.indexOf(' '));
        args = input.substring(input.indexOf(' ') + 1);
    }
    
    findAndExecute(cmd, args);
}
```

### Built-in Commands
The service provides 3 built-in commands:
1. HELP - Lists all registered commands
2. REBOOT - Restarts ESP32
3. MEM - Shows memory statistics

All other commands will be registered by Application.cpp.

## Flash Impact

### Test Sketch 58
- **Flash Used:** 306,291 bytes (23%)
- **RAM Used:** 20,840 bytes (6%)
- **Target:** <400KB ✓
- **Comparison:** 77% smaller than v4.1 baseline (92%)

### Dependencies
- Arduino.h (standard)
- std::vector (C++ STL)
- std::function (C++ STL)

**No service dependencies** - pure infrastructure layer.

## Testing Strategy

### Automated Tests
1. Singleton pattern verification
2. Command registration count
3. Non-blocking behavior

### Interactive Tests
1. Basic commands (HELLO, COUNT)
2. Space-separated arguments (ADD 5 3)
3. Colon-separated arguments (SET:KEY=VALUE)
4. Case-insensitive matching (hello vs HELLO)
5. Unknown command handling
6. Built-in commands (HELP, MEM, REBOOT)

## Integration Notes

### Phase 3 Next Steps
1. ConfigService will implement CONFIG, SET_CONFIG, SAVE_CONFIG
2. OrchestratorService will implement STATUS, QUEUE_TEST, FORCE_UPLOAD
3. TokenService will implement TOKENS, TEST_VIDEO
4. Application will implement SIMULATE_SCAN, START_SCANNER

### Command Migration Plan
Each service registers its own commands during initialization:
```cpp
void Application::setup() {
    _serial.begin();
    _serial.registerBuiltinCommands();
    
    _config.registerCommands(_serial);    // CONFIG, SET_CONFIG, SAVE_CONFIG
    _tokens.registerCommands(_serial);    // TOKENS, TEST_VIDEO
    _orchestrator.registerCommands(_serial); // STATUS, QUEUE, etc.
    
    registerApplicationCommands();         // SIMULATE_SCAN, START_SCANNER
}
```

## Code Quality Metrics

- **Total Lines:** 278 (header-only)
- **Effective Code:** ~200 (excluding comments/whitespace)
- **Largest Method:** findAndExecute() - 10 lines
- **Singleton Pattern:** ✓ Correct
- **RAII Cleanup:** N/A (no resources to manage)
- **Thread Safety:** Single-threaded (main loop only)

## Known Limitations

1. **Linear Search:** O(n) command lookup
   - Acceptable for <50 commands
   - Could use std::map if needed
   
2. **String Allocation:** Each command creates String objects
   - Minimal impact (commands are rare events)
   - Alternative: const char* with strcasecmp
   
3. **No Command History:** Serial input is not buffered
   - Could add readline-style history
   - Not needed for production use

## Success Criteria

✓ Compiles without errors
✓ Flash usage <400KB (achieved 306KB)
✓ Command registry pattern implemented
✓ std::function callbacks work
✓ Non-blocking processing
✓ Singleton pattern correct
✓ Zero service dependencies
✓ Case-insensitive matching
✓ Argument parsing (space and colon)
✓ Unknown command handling
✓ Built-in commands (HELP, REBOOT, MEM)

## References

- Agent Spec: `.claude/agents/serial-service-extractor.md`
- Refactor Guide: `REFACTOR_IMPLEMENTATION_GUIDE.md` (lines 1075-1088)
- v4.1 Source: `ALNScanner1021_Orchestrator/ALNScanner1021_Orchestrator.ino` (lines 2927-3394)
