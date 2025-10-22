# Speckit Slash Commands Status

**Date**: October 18, 2025
**Issue**: Slash commands not appearing in Claude Code autocomplete

## 📋 Current Status

### ✅ Commands ARE Correctly Configured
Your speckit commands exist and are properly formatted:

```
.claude/commands/
├── speckit.analyze.md
├── speckit.checklist.md
├── speckit.clarify.md
├── speckit.constitution.md
├── speckit.implement.md
├── speckit.plan.md
├── speckit.specify.md
└── speckit.tasks.md
```

All 8 commands have proper YAML frontmatter with descriptions.

### ❌ Known Bug in Claude Code

This is **Issue #9518** in the Claude Code repository (filed Oct 14, 2025):
- **Platform**: Linux (also affects Windows)
- **Symptom**: Custom slash commands in `.claude/commands/` not discovered
- **Status**: Active bug, being tracked by Anthropic

Multiple related issues throughout 2025:
- #9518 (Oct 14) - Linux
- #8831 (Oct 3) - ~/.claude/commands/ not loading
- #7283 (Sept 7) - Windows

## 🔧 Workarounds

### Option 1: Direct Invocation (May Work)
Even if autocomplete doesn't show them, try typing the full command:

```
/speckit.plan
/speckit.tasks
/speckit.implement
/speckit.specify
/speckit.clarify
/speckit.analyze
/speckit.checklist
/speckit.constitution
```

### Option 2: Restart Claude Code
Sometimes a full restart helps reload command definitions:
1. Exit Claude Code completely
2. Restart your session
3. Navigate back to this directory

### Option 3: Manual Execution
Until the bug is fixed, you can manually trigger the workflow by:
1. Reading the command file content
2. Following the instructions manually
3. Example: `cat .claude/commands/speckit.plan.md`

## 📁 Command Descriptions

| Command | Purpose |
|---------|---------|
| `/speckit.specify` | Create or update feature specification from natural language |
| `/speckit.plan` | Execute implementation planning workflow using plan template |
| `/speckit.tasks` | Generate actionable, dependency-ordered tasks.md |
| `/speckit.implement` | Execute implementation plan by processing tasks.md |
| `/speckit.clarify` | Identify underspecified areas and ask clarification questions |
| `/speckit.analyze` | Perform consistency analysis across spec/plan/tasks |
| `/speckit.checklist` | Generate custom checklist for current feature |
| `/speckit.constitution` | Create/update project constitution |

## 🐛 Bug Tracking

**GitHub Issue**: https://github.com/anthropics/claude-code/issues/9518

**Common Symptoms**:
- No error messages in Claude Code
- Commands simply not detected
- Typing `/` only shows built-in commands
- "Unknown slash command" error when attempting to invoke

**Confirmed Working**:
- Hooks in `.claude/settings.json` work fine
- File permissions are correct
- YAML frontmatter is properly formatted

## 🔍 What We've Ruled Out

✅ **NOT** a configuration issue:
- All files have correct YAML frontmatter
- Descriptions are present and properly formatted
- File permissions are correct (644)
- Files are UTF-8 encoded

✅ **NOT** a directory issue:
- `.claude/commands/` directory exists
- No conflicts with additional directories
- No stale references in configuration

✅ **NOT** a filesystem migration issue:
- Cleaned up all stale WSL2/Windows paths
- Updated `.claude/settings.local.json`
- Removed obsolete references

## 📝 What We Cleaned Up

During this investigation, we removed stale references from the previous WSL2 setup:

1. **`.claude/settings.local.json`**
   - Removed: `/mnt/c/Users/spide/...` path
   - Removed: `powershell.exe` permission

2. **`CYD_COMPATIBILITY_STATUS.md`**
   - Updated: WSL2 → Raspberry Pi native setup
   - Updated: COM ports → `/dev/ttyUSB0` serial ports

3. **`specs/001-we-are-trying/`**
   - Updated: Arduino CLI commands for native Linux
   - Updated: Serial monitor commands
   - Added deprecation notices to historical docs

4. **`specs/002-audio-debug-project/`**
   - Updated: Commands for Raspberry Pi
   - Added historical document notices

## 🎯 Next Steps

1. **Wait for Fix**: Monitor Issue #9518 for updates
2. **Try Workarounds**: Attempt direct command invocation
3. **Manual Workflow**: Use the speckit workflows manually until fixed

## 📚 Related Documentation

- See `archive/deprecated-docs/WSL2_ARDUINO_SETUP.md` for old WSL2 setup
- See `CLAUDE.md` for current Raspberry Pi Arduino development environment
- See `.specify/` directory for the underlying speckit workflow scripts

---

*This is a known upstream bug, not a configuration issue with your project.*
