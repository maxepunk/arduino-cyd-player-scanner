/**
 * SerialService.h
 *
 * Interactive serial command interface with command registry pattern.
 * Provides infrastructure for registering and executing debug commands.
 *
 * Architecture:
 * - Command registry pattern (replaces if/else chains)
 * - std::function callbacks for flexibility
 * - Non-blocking command processing
 * - Singleton pattern for global access
 * - Zero dependencies on other services (pure infrastructure)
 *
 * Extracted from: ALNScanner v4.1 lines 2927-3394 (processSerialCommands)
 * Target: ~200 lines header-only implementation
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>

namespace services {

class SerialService {
public:
    // Command handler callback type
    // Args: command arguments (text after command name)
    using CommandHandler = std::function<void(const String& args)>;

    // Singleton access
    static SerialService& getInstance() {
        static SerialService instance;
        return instance;
    }

    // ─── Lifecycle ───────────────────────────────────────────────────

    /**
     * Initialize serial communication
     * @param baudRate Serial baud rate (default: 115200)
     */
    void begin(uint32_t baudRate = 115200) {
        if (_initialized) {
            return;
        }

        Serial.begin(baudRate);
        _initialized = true;

        // Wait for serial to stabilize
        delay(100);

        Serial.println();
        Serial.println(F("═══════════════════════════════════════════════"));
        Serial.println(F("  ALNScanner v5.0 - Serial Command Interface"));
        Serial.println(F("═══════════════════════════════════════════════"));
        Serial.println();
        Serial.printf("Initialized at %lu baud\n", baudRate);
        Serial.println(F("Type 'HELP' for available commands"));
        Serial.println();
    }

    // ─── Command Registration ────────────────────────────────────────

    /**
     * Register a command with callback handler
     * @param name Command name (case-insensitive)
     * @param handler Function to execute when command is invoked
     * @param help Help text describing the command
     */
    void registerCommand(const String& name, CommandHandler handler, const String& help = "") {
        if (!handler) {
            Serial.printf("[SERIAL] Warning: Null handler for command '%s'\n", name.c_str());
            return;
        }

        // Check for duplicate and replace if exists
        for (size_t i = 0; i < _commands.size(); i++) {
            if (_commands[i].name.equalsIgnoreCase(name)) {
                _commands[i].handler = handler;
                _commands[i].help = help;
                return;
            }
        }

        // Add new command
        Command cmd;
        cmd.name = name;
        cmd.handler = handler;
        cmd.help = help;
        _commands.push_back(cmd);
    }

    /**
     * Register built-in commands (HELP, REBOOT, etc.)
     */
    void registerBuiltinCommands() {
        // HELP command - shows all registered commands
        registerCommand("HELP", [this](const String& args) {
            Serial.println();
            Serial.println(F("═══════════════════════════════════════════════"));
            Serial.println(F("       AVAILABLE COMMANDS"));
            Serial.println(F("═══════════════════════════════════════════════"));
            Serial.println();

            if (_commands.empty()) {
                Serial.println(F("No commands registered"));
            } else {
                for (const auto& cmd : _commands) {
                    Serial.printf("  %-20s", cmd.name.c_str());
                    if (cmd.help.length() > 0) {
                        Serial.printf(" - %s", cmd.help.c_str());
                    }
                    Serial.println();
                }
                Serial.println();
                Serial.printf("Total: %d commands\n", _commands.size());
            }

            Serial.println(F("═══════════════════════════════════════════════"));
            Serial.println();
        }, "Show this help message");

        // REBOOT command - restart ESP32
        registerCommand("REBOOT", [](const String& args) {
            Serial.println();
            Serial.println(F("═══════════════════════════════════════════════"));
            Serial.println(F("       REBOOTING DEVICE"));
            Serial.println(F("═══════════════════════════════════════════════"));
            Serial.println(F("Restarting ESP32 in 2 seconds..."));
            Serial.flush();

            delay(2000);

            Serial.println(F("\n>>> Restarting now..."));
            Serial.flush();
            delay(100);

            esp_restart();
        }, "Restart ESP32");

        // MEM command - show memory info
        registerCommand("MEM", [](const String& args) {
            Serial.println();
            Serial.println(F("=== Memory Status ==="));
            Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
            Serial.printf("Heap size: %d bytes\n", ESP.getHeapSize());
            Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz());
            Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
            Serial.println(F("====================="));
            Serial.println();
        }, "Show memory and system info");

        Serial.printf("[SERIAL] Registered %d built-in commands\n", 3);
    }

    // ─── Command Processing ──────────────────────────────────────────

    /**
     * Process incoming serial commands (call in loop())
     * Non-blocking - returns immediately if no data available
     */
    void processCommands() {
        if (!_initialized) {
            return;
        }

        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();

            if (input.length() > 0) {
                handleCommand(input);
            }
        }
    }

    // ─── Debug ────────────────────────────────────────────────────────

    /**
     * Print all registered commands (for debugging)
     */
    void printRegisteredCommands() const {
        Serial.printf("[SERIAL] Registered commands (%d total):\n", _commands.size());
        for (const auto& cmd : _commands) {
            Serial.printf("  - %s", cmd.name.c_str());
            if (cmd.help.length() > 0) {
                Serial.printf(" (%s)", cmd.help.c_str());
            }
            Serial.println();
        }
    }

private:
    // ─── Singleton Pattern ────────────────────────────────────────────

    SerialService() = default;
    ~SerialService() = default;
    SerialService(const SerialService&) = delete;
    SerialService& operator=(const SerialService&) = delete;

    // ─── Internal Data Structures ─────────────────────────────────────

    struct Command {
        String name;            // Command name (e.g., "STATUS")
        CommandHandler handler; // Callback function
        String help;            // Help text
    };

    std::vector<Command> _commands;
    bool _initialized = false;

    // ─── Internal Command Processing ─────────────────────────────────

    /**
     * Parse and execute a command
     * @param input Raw command line (e.g., "STATUS" or "SET_CONFIG:KEY=VALUE")
     */
    void handleCommand(const String& input) {
        // Split command and arguments
        String cmd = input;
        String args = "";

        // Handle colon-separated commands (e.g., "SET_CONFIG:KEY=VALUE")
        int colonIndex = input.indexOf(':');
        if (colonIndex != -1) {
            cmd = input.substring(0, colonIndex);
            args = input.substring(colonIndex + 1);
        } else {
            // Handle space-separated commands (e.g., "ADD 5 3")
            int spaceIndex = input.indexOf(' ');
            if (spaceIndex != -1) {
                cmd = input.substring(0, spaceIndex);
                args = input.substring(spaceIndex + 1);
            }
        }

        cmd.trim();
        args.trim();

        // Echo command (useful for logging)
        Serial.printf("\n[CMD] %s", cmd.c_str());
        if (args.length() > 0) {
            Serial.printf(" (args: %s)", args.c_str());
        }
        Serial.println();

        // Execute command
        if (!findAndExecute(cmd, args)) {
            Serial.println();
            Serial.printf("✗ Unknown command: '%s'\n", cmd.c_str());
            Serial.println(F("Type 'HELP' for available commands"));
            Serial.println();
        }
    }

    /**
     * Find and execute a registered command
     * @param cmd Command name (case-insensitive)
     * @param args Command arguments
     * @return true if command was found and executed
     */
    bool findAndExecute(const String& cmd, const String& args) {
        // Linear search with case-insensitive comparison
        for (const auto& command : _commands) {
            if (command.name.equalsIgnoreCase(cmd)) {
                // Execute handler
                command.handler(args);
                return true;
            }
        }
        return false;
    }
};

} // namespace services
