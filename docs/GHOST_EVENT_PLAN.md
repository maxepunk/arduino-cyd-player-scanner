# Ghost Event Scanner — Implementation Plan

**Branch:** `ghost-event` (cut from `main` @ `af74fbd`)
**Repo:** `github.com/maxepunk/arduino-cyd-player-scanner`
**Created:** 2026-08-27
**Status:** approved, implementation not yet started

---

## 1. Goal

Repurpose the CYD ESP32 player scanner for a **different event**, unrelated to
About Last Night. The device is a standalone prop:

1. A guest places a "ghost" (an NFC-tagged prop) on the scanner.
2. The scanner reads the tag and plays the ghost's audio, showing an image if
   one exists for that ghost.
3. That is the entire feature set. **No orchestrator, no network, no scoring,
   no video, no session.**

One device is being built. It will be **shipped to a non-technical person**
who sources their own microSD card, creates the audio, and loads the card.
That person does not have access to the build machine.

---

## 2. Constraint: no remnants on the ALN side

This work must not leave confusing state in the ALN ecosystem.

- Nothing is committed to the submodule's `main`.
- Nothing is committed to the `ALN-Ecosystem` parent repo, **including its
  submodule gitlink**.
- Because checking out a branch inside the submodule makes the parent report
  `modified: arduino-cyd-player-scanner (new commits)`, the work happens in a
  **git worktree** at `~/projects/ghost-scanner`. The submodule checkout inside
  `ALN-Ecosystem` stays on `main`.

Verified working: git 2.39.5, submodule gitdir at
`.git/modules/arduino-cyd-player-scanner`.

---

## 3. Decision record

Decisions reached by structured interview before any code was written.

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | Branch in the existing submodule repo, not a new repo | Keeps shared history so hardware fixes cherry-pick both ways |
| 2 | Delete orchestrator code outright; `#ifdef` only where logic interleaves | Six service headers are cleanly separable; nothing in `hal/` or `ui/` depends on them |
| 3 | Audio accepted as working; no hardware proof required first | Verified on previous iterations of the scanner |
| 4 | Keep NDEF text IDs and keep the unknown-token gate | Proven on this hardware; UID fallback was deliberately removed in v5 |
| 5 | Antenna sits **below** the portrait screen; arrow points down | Confirmed against the physical build |
| 6 | Auto-dismiss when audio ends, tap to override | Prop is unattended; a stale ghost must clear itself |
| 7 | Boot override window 30s -> **10s**, and paint the home screen before it | Prop must look alive quickly; serial escape hatch preserved |
| 8 | `config.txt` slimmed to `DEVICE_ID`, `DEBUG_MODE`, `VOLUME` | Everything else is inert once networking is gone |
| 9 | Asset prep documented via **Audacity**, not a toolchain | Card-maker is non-technical, on Windows/macOS, without the build machine |
| 10 | Unknown / unusable tag copy: **"THAT'S NO SPIRIT, TRY AGAIN"** | In-world; an error message reads as a broken machine |
| 11 | Home screen static, drawn ghost via TFT primitives | Text-art misaligns at this size; animation couples to the RFID poll loop |
| 12 | All ghosts share one screen treatment; audio is the differentiation | No per-ghost captions or images required |
| 13 | Keep the SAK 0x00 tag gate; specify NTAG213/215/216 in docs | MIFARE Classic support means auth/sector keys, days of work |
| 14 | `VOLUME` exposed in `config.txt` | "Too quiet in the room" is the most predictable field complaint |
| 15 | RFID stays blocked during playback; one ghost at a time | User's call; clip length is therefore the walk-away dead-time budget |
| 16 | Single tap dismisses during playback | Safe now that the status screen needs a 5s long-press |
| 17 | Hidden status screen via **5-second long-press** | Single tap too easily triggered by a guest |
| 18 | Vendor libraries in-repo and pin the core version | Makes the branch build anywhere and unlocks a CI compile job |

---

## 4. Hard limits (established from the code, not the docs)

These are the constraints content must be designed against.

### Audio — `AudioGeneratorWAV`

| Constraint | Value |
|---|---|
| Container | Uncompressed **PCM `.wav` only** (no MP3 decoder compiled in) |
| Bit depth | **8 or 16-bit only** — explicit reject otherwise |
| Channels | Mono or stereo (1-2) |
| Sample rate | No code limit; CPU is the practical ceiling |
| Duration | **No code limit** — streams from SD, bounded by card capacity |
| Output | ESP32 **internal DAC (8-bit)** -> small onboard speaker |

**Published recipe: 16-bit PCM WAV, mono, 22050 Hz.** ~2.65 MB/minute.

Longer clips work. The trade-off is that RFID is blocked for the clip's
duration, so clip length is the prop's dead time after a guest walks away.

### Images — `drawBMP`

- **24-bit uncompressed BMP only** (32-bit alpha and RLE rejected).
- Dimensions are **not validated** — the header is trusted and a `width * 3`
  row buffer is malloc'd. 240x320 fills the portrait screen; smaller renders
  into the top-left corner; wider than 240 runs off.
- Images are **optional** per ghost.

### Tags

- **NTAG213 / 215 / 216 only.** `parseNDEFText()` hard-rejects `SAK != 0x00`.
- **MIFARE Classic will not work** — and it is the most commonly sold cheap
  NFC card, so this must be stated prominently in the README.
- Tag must carry an **NDEF text record** containing the tokenId.

---

## 5. Runtime behaviour

Resolved scan outcomes. Image presence is checked with `SD.exists()` up front
rather than letting `drawBMP` fail and paint its own error.

| Image | Audio | Screen | Dismiss |
|-------|-------|--------|---------|
| —     | yes   | Drawn ghost | Auto at audio end, or single tap |
| yes   | yes   | The BMP     | Auto at audio end, or single tap |
| yes   | —     | The BMP     | Single tap only |
| —     | —     | "THAT'S NO SPIRIT, TRY AGAIN" | Auto after 1.5s |

The last row is the setup-error case: tag valid, card has nothing for it. It
reuses the existing non-blocking `showScanFailed()` path so RFID is not
blocked and the guest can retry immediately. A distinct serial log line
distinguishes it from "tag not recognised" for whoever is debugging the card.

**Implementation note:** `_audioStarted` is currently set false both when audio
finishes and when it never started. `isAudioPlaying()` cannot distinguish
"clip ended" from "file missing", so the auto-dismiss logic must track these
separately or a broken file reads as an instantly-completed clip.

---

## 6. Firmware changes

### 6.1 Deletions

**Orchestrator services:** `OrchestratorService.h`, `AssetService.h`,
`AssetManifestDiff.h`, `BatchId.h`, `PayloadBuilder.h`, `ScanResponse.h`.

**Video concept:** `ui/screens/ProcessingScreen.h`; the `video` field and
`isVideoToken()` from `models/Token.h`; `PROCESSING_VIDEO` and
`showProcessing()` from `ui/UIStateMachine.h`.

**From `Application.h`:** orchestrator send/queue block in the scan path,
`applyScanOutcome()`, `generateTimestamp()`, `startBackgroundTasks()` and the
Core-0 task, WiFi/token-sync/asset-sync in `initializeServices()`, the
"NeurAI / Memory Scanner / v5.0 Booting..." splash, and the TFT boot status
prints. The `No SD Card!` error stays — a card-less prop is genuinely broken.

**Serial commands:** `QUEUE_TEST`, `FORCE_OVERFLOW`, `FORCE_UPLOAD`,
`SHOW_QUEUE`, `QUEUE_STATUS`, `CLEAR_QUEUE`, `SYNC_ASSETS_NOW`,
`DIAG_NETWORK`. `SIMULATE_SCAN` stays, rewired to drive the display path
directly — it is the only way to test without tags.

After this strip, `WiFi.h`, `HTTPClient.h`, `WiFiClientSecure.h` and
`mbedtls/sha1.h` have no remaining includers; the whole TLS/network stack
leaves the build.

### 6.2 Modifications

| File | Change |
|------|--------|
| `models/Config.h` | `validate()` drops SSID/password/URL/TEAM_ID rules, keeps DEVICE_ID length; add `volume` |
| `services/ConfigService.h` | Parse/save only `DEVICE_ID`, `DEBUG_MODE`, `VOLUME` |
| `config.h` | `DEBUG_OVERRIDE_TIMEOUT_MS` 30000 -> 10000; delete vestigial `AUDIO_BCLK/LRC/DIN` constants |
| `Application.h` | Paint home screen before the countdown; new scan flow (NDEF -> gate -> asset presence -> display or failure) |
| `hal/AudioDriver.h` | Call `SetGain(config.volume)` after creating `_output` |
| **new** `ui/screens/GhostReadyScreen.h` | "PLACE GHOST HERE" top, drawn ghost centre, `fillTriangle` arrow at bottom edge, static, black |
| `ui/screens/TokenDisplayScreen.h` | Remove the `delay(1000)` splash; `SD.exists()` image check; track `_hasImage`/`_audioStarted` separately; auto-dismiss distinguishing finished vs never-started |
| `ui/UIStateMachine.h` | Single-tap dismiss in `DISPLAYING_TOKEN`; 5-second long-press -> status; remove double-tap |
| `ui/screens/ScanFailedScreen.h` | Copy -> "THAT'S NO SPIRIT, TRY AGAIN" |
| `ui/screens/StatusScreen.h` | Terse: DEVICE_ID, token count, RFID state, volume |

### 6.3 Vestigial constants worth deleting

`config.h` declares `AUDIO_BCLK = 26`, `AUDIO_LRC = 25`, `AUDIO_DIN = 22` and
CLAUDE.md documents `AUDIO_DIN` as conflicting with `RFID_SCK`. **These
constants are referenced nowhere in the codebase.** Audio uses
`AudioOutputI2S(0, 1)` — internal DAC mode on GPIO 25/26 — so the documented
pin conflict does not exist. Delete the constants on this branch.

---

## 7. Libraries and build reproducibility

The build resolves libraries from `~/Arduino/libraries`, **not** the repo's
`libraries/` folder. All four in-repo copies were verified byte-identical to
the installed ones at the same versions, so vendoring is already effectively
done — only the build command needs to point at them.

**Changes on this branch (DONE):**

- **Added** `libraries/ArduinoJson/` (7.4.2, 3.2 MB). It was the only
  dependency not vendored. The v6/v7 distinction matters: v7's elastic
  documents silently ignore the capacity argument v6 required, which caused
  the asset-sync heap bug.
- **Removed** `libraries/ESP32-audioI2S-master/`, `libraries/XPT2046_Bitbang/`,
  `libraries/XPT2046_Touchscreen/`. All unused — `TouchDriver.h` includes only
  `Arduino.h` and `config.h` and bit-bangs SPI itself.

**Verified empirically**, not assumed. A clean verbose build with
`--libraries ./libraries` reported these include paths:

```
-I<worktree>/libraries/ESP8266Audio/src     vendored
-I<worktree>/libraries/MFRC522/src          vendored
-I<worktree>/libraries/TFT_eSPI             vendored
-I~/Arduino/libraries/ArduinoJson/src       FELL BACK to the machine
```

So `--libraries` does take precedence over the user sketchbook, and the
ArduinoJson gap was real: **the CI compile job would have failed on a clean
runner** before it was vendored.

After vendoring, a repeat clean build resolved all four in-repo with no
fallback, exit 0, 59% flash unchanged:

```
-I<worktree>/libraries/ArduinoJson
-I<worktree>/libraries/ESP8266Audio
-I<worktree>/libraries/MFRC522
-I<worktree>/libraries/TFT_eSPI
```

Re-verify this resolution table after any change to the library set.
- **Keep** `ESP8266Audio`, `MFRC522`, `TFT_eSPI` (the last carries the
  CYD-critical `User_Setup.h`: `ST7789_DRIVER`, `TFT_RGB_ORDER TFT_BGR`,
  `TFT_BL 21`).
- Build with `--libraries ./libraries`.

The **esp32 core** cannot reasonably be vendored; it stays an
`arduino-cli core install esp32:esp32@3.3.2` step, pinned by version.

### Build command

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PartitionScheme=no_ota,UploadSpeed=921600 \
  --libraries ./libraries \
  ALNScanner_v5
```

---

## 8. New files

- **`README.md`** — rewritten, split by audience.
  - *Making a card* (the remote, non-technical person): which tags to buy
    (NTAG213/215/216; **MIFARE Classic will not work**), writing tags with the
    NFC Tools phone app, the Audacity export recipe as exact menu paths, copying
    `card-template/` to a blank card, naming WAVs to match tokenIds, editing
    `tokens.json`, and a troubleshooting table.
  - *Developer*: build and flash commands, serial access via the 10s override,
    the pinned core version, relationship to `main`.
- **`card-template/`** — `config.txt` (DEVICE_ID, DEBUG_MODE, VOLUME),
  `tokens.json` pre-filled with the shipped tag IDs, and
  `assets/audio/` + `assets/images/` with `.gitkeep`.
- **`scripts/prepare-ghost-audio.sh`** — ffmpeg wrapper producing the exact
  target format. **Developer-side convenience only**; the documented path for
  the card-maker is Audacity.

---

## 9. Tests and CI

- **Delete** `test/test_payload/`, `test/test_scan_response/`,
  `test/test_batch_id/`, `test/test_asset_manifest/` — they test deleted code.
- **Rewrite** `test_config` for the new validation rules plus `VOLUME` parsing.
- **Update** `test_token` for the removed `video` field.
- **Keep** `test_ndef` and `test_smoke` untouched. `test_ndef` matters more
  here than on `main`: NDEF parsing is now the only thing between a tag and a
  display.
- **Add an `arduino-cli compile` job to CI.** CI has only ever run native
  logic tests, never a cross-compile — which is why the "won't compile on core
  3.x" asset-sync bug reached hardware. Vendored libraries plus a pinned core
  make this job a few lines.

---

## 10. Verification

1. `pio test -e native` green.
2. `arduino-cli compile` clean; record flash %. Expect well below the current
   59% once the network stack is gone.
3. Flash to the device.
4. On hardware: boot to home screen in under 10s; ghost renders; tag scan
   plays audio; auto-dismiss at clip end; single-tap dismiss; 5-second
   long-press reveals status; unrecognised tag shows "THAT'S NO SPIRIT".

---

## 11. Open items

1. **No microSD card in the device.** Firmware currently halts at early
   hardware init (`SD card mount failed - no card present`). Compile and flash
   can proceed; on-hardware verification cannot.
2. **Ghost count and tokenIds** unknown. Needed to pre-fill
   `card-template/tokens.json`. Defaulting to `ghost01`..`ghostNN` with
   documented instructions for changing them.
3. **Content not yet designed** — the limits in section 4 exist to be handed
   to whoever designs it.
4. **`VOLUME` default** 1.0, documented useful range 0.0-2.0. `SetGain`
   accepts up to 4.0 but clips hard.
