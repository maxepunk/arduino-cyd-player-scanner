# ESP32 Token Data Flow Analysis - Document Index

This directory contains a comprehensive end-to-end analysis of token data flowing from ALN-TokenData submodule through the backend to ESP32 devices.

## Documents

### 1. TOKEN_FLOW_SUMMARY.md (9.4 KB, 273 lines)
**Quick Reference - Start Here**

Executive summary with:
- Complete data flow diagram (6 stages: Source → Load → API → Download → Load → Runtime)
- Critical findings (5 major discoveries)
- Synchronization mechanisms (boot-time only)
- Data transformations (4 stages)
- Issues and gaps (5 identified)
- Recommendations (13 prioritized actions)
- File path references (6 key locations)

**Read this if:** You need quick answers or overview

### 2. ESP32_TOKEN_DATA_FLOW_ANALYSIS.md (40 KB, 1181 lines)
**Complete Technical Reference**

Comprehensive deep-dive with:
- 13 detailed sections with code snippets
- Token data source documentation (ALN-TokenData structure)
- Backend loading mechanisms (loadRawTokens vs loadTokens)
- /api/tokens endpoint specification
- ESP32 boot initialization sequence
- HTTP download implementation (with size limits)
- SD card caching architecture
- Complete synchronization flow
- Data format at each stage (6 stages documented)
- Size constraint analysis (current vs projected)
- Complete data flow diagram with all layers
- All issues with severity levels
- Detailed recommendations with rationales

**Read this if:** You need complete technical understanding

## Key Findings Summary

### Token Source
- **Location:** `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/ALN-TokenData/tokens.json`
- **Size:** 12 KB (371 lines, ~42 tokens)
- **Format:** JSON object map (tokenId → token object with SF_RFID, SF_ValueRating, image, audio, video, processingImage, SF_MemoryType, SF_Group)

### Synchronization Strategy
- **Timing:** Boot-time only (not runtime)
- **Mechanism:** HTTP GET `/api/tokens` → Save to `/tokens.json` on SD card
- **Fallback:** Uses cached version if download fails
- **Configuration:** `config.txt` setting `SYNC_TOKENS=true` (default) or false
- **Gap:** No automatic updates during operation (would require ESP32 reboot to get new tokens)

### Size Feasibility
| Metric | Current | Limit | Status |
|--------|---------|-------|--------|
| tokens.json | 12 KB | 50 KB HTTP | Safe ✅ |
| Token count | 42 | 200+ | Safe at 100 ⚠️ |
| RAM at runtime | 2 KB | Unlimited | Trivial ✅ |
| Flash impact | 0 KB | 92% full | SD cached ✅ |

### Data Flow Stages
1. **Source:** ALN-TokenData/tokens.json
2. **Backend Load:** tokenService.js (loads via submodule path)
3. **API Serving:** /api/tokens HTTP endpoint (wrapper with count + timestamp)
4. **ESP32 Download:** Boot-time sync via HTTP GET
5. **SD Cache:** /tokens.json (byte-for-byte copy of HTTP response)
6. **Runtime:** TokenService vector lookup (tokenId + video field only)

### Critical Paths
- Backend → Source: `backend/src/services/tokenService.js` (lines 49-115)
- Backend → API: `backend/src/routes/resourceRoutes.js` (lines 16-31)
- ESP32 → Download: `OrchestratorService.h` (lines 203-268)
- ESP32 → Load: `TokenService.h` (lines 93-146)
- ESP32 → Initialize: `Application.h` (lines 818-844)

### Identified Issues (5 total)
1. **No runtime updates** (Medium) - Tokens only sync at boot
2. **No validation on download** (Low) - Could add JSON schema check
3. **No timestamp verification** (Low) - Could compare version before caching
4. **Growth path unclear** (Low) - 50KB limit adequate for 100 tokens, problematic at 200+
5. **Configuration underutilized** (Low) - SYNC_TOKENS rarely documented

## Quick Navigation

**For System Architects:** Read TOKEN_FLOW_SUMMARY.md sections: Critical Findings, Data Flow

**For Backend Developers:** Read ESP32_TOKEN_DATA_FLOW_ANALYSIS.md sections: 2, 3 (Backend Service, API Endpoint)

**For ESP32 Developers:** Read ESP32_TOKEN_DATA_FLOW_ANALYSIS.md sections: 4, 5, 6 (ESP32 Reception, Synchronization, Transformations)

**For DevOps/Operations:** Read TOKEN_FLOW_SUMMARY.md sections: Synchronization Mechanisms, Recommendations

**For QA/Testing:** Read ESP32_TOKEN_DATA_FLOW_ANALYSIS.md section: 9 (Issues and Gaps)

## Project Context

**Project:** About Last Night (ALN) - Memory Token Scanning System
**Scope:** 2-hour live event with RFID scanners
**Architecture:** 
- Backend Node.js orchestrator (loads tokens from submodule)
- ESP32 hardware scanner (downloads tokens on boot, caches to SD)
- Web scanners (nested submodule data)

## Recommendations at a Glance

### Immediate (Do First)
1. Document token sync is boot-time only
2. Add Application.h comments explaining sync flow

### Short-term (Next Sprint)
3. Add JSON validation on download
4. Use temp file + atomic rename (avoid corruption on partial write)
5. Add logging of token hash after sync

### Medium-term (Planning)
6. Expose manual `FORCE_SYNC` command for testing
7. Add periodic background polling (optional, every 5 min)

### Long-term (When Scaling)
8. Implement pagination if tokens exceed 100
9. Consider gzip compression

---

**Analysis Completed:** October 31, 2025
**Analysis Thoroughness:** Very detailed with code tracing through all 6 stages
**Verification:** Complete end-to-end flow traced with file paths and line numbers
