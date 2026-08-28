# Ghost Scanner

A standalone prop. A guest places a ghost on the scanner; the scanner
recognises it and plays that ghost's audio, showing a picture if one has
been provided.

That is the whole device. It does not connect to WiFi, a network, or any
server. Everything it knows comes from the microSD card inside it.

---

**Part 1** is for whoever prepares the microSD card and the ghosts. It
assumes no programming and nothing to install.
**Part 2** is for whoever builds and flashes the firmware.

---

# Part 1 — Making a card

## What you need

| | |
|---|---|
| A microSD card | Any size. 8GB is far more than enough. |
| NFC tags | **NTAG213**, NTAG215 or NTAG216. See the warning below. |
| A phone | Android, or an iPhone that can write NFC tags. |
| A computer | Windows or macOS. |
| The Ghost Asset Conformer | A folder you'll be sent. Nothing to install. |

> ### ⚠️ Buy the right tags
>
> The scanner can **only** read **NTAG213 / NTAG215 / NTAG216** tags.
>
> **MIFARE Classic and MIFARE 1K tags will NOT work.** They are the most
> commonly sold cheap NFC tags, so it is genuinely easy to buy the wrong
> ones. If you search for "NFC tags", check the listing says NTAG213 (or
> 215/216) before ordering.
>
> A wrong tag is not subtly wrong — it will never be recognised, and the
> scanner will show *THAT'S NO SPIRIT* every single time.

## Overview

```
    your sounds  ──►  Conformer  ──►  2-READY-FOR-CARD  ──►  microSD card
    your images       (double-click)   (everything ready)
```

You make sounds and pictures however you like, in whatever format your
software produces. The Conformer converts them into the exact formats the
scanner needs, and writes the ghost list for you.

## Step 1 — Name your files after your ghosts

**The filename is the ghost's name.** That is the one idea that makes
everything else work.

If you want a ghost called `ghost01`, call its sound file `ghost01`:

```
    ghost01.mp3
    ghost02.wav
    ghost03.m4a
```

Almost any sound format is fine — mp3, wav, m4a, aac, flac, ogg, aiff.

Pictures are **optional**. Without one, the scanner draws its own ghost on
screen, which is the normal look for this build. If you do want a picture
for a ghost, give it the same name:

```
    ghost01.png
```

Almost any picture format is fine — png, jpg, bmp, gif, tif, webp.

Capitals and spaces are tidied up for you: `Ghost 01.mp3` becomes `ghost01`.

## Step 2 — Run the Conformer

1. Unzip the **Ghost Asset Conformer** folder somewhere easy, like your
   Desktop.
2. Put all your sound and picture files into the folder called
   **`1-PUT-YOUR-FILES-HERE`**.
3. Double-click **`CONFORM MY FILES`**.

A black window opens and lists each file as it converts. When it says
**Done**, close it.

> **If it won't open:** on a Mac, right-click (or Control-click) the file
> and choose **Open**, then **Open** again. On Windows, click **More info**
> then **Run anyway**. You only need to do this once. Neither warning means
> anything is wrong — they appear for any program not bought from Apple or
> Microsoft.

The Conformer tells you which ghosts it found. Check that list matches what
you expect before moving on.

## Step 3 — Copy everything onto the card

1. Format the microSD card as **FAT32** (or **MS-DOS (FAT)** on a Mac).
2. Open the **`2-READY-FOR-CARD`** folder.
3. Copy **everything inside it** onto the card.

The card should end up looking like this, with `config.txt` and
`tokens.json` at the top level and not inside a folder:

```
    (your SD card)
    ├── config.txt
    ├── tokens.json
    └── assets
        ├── audio
        │   ├── ghost01.wav
        │   └── ghost02.wav
        └── images
            └── ghost01.bmp
```

## Step 4 — Write the tags

Install **NFC Tools** (free, Android and iOS).

For each ghost:

1. Open NFC Tools → **Write** → **Add a record**
2. Choose **Text**
3. Type the ghost's name exactly as the filename: `ghost01`
4. Tap **OK**, then **Write**, and hold the tag against your phone

Use **Text**, not "URL", not "Data". The scanner reads text records only.

To check a tag afterwards, use the **Read** tab — it should show your text
and identify the tag as NTAG213 (or 215/216).

## Step 5 — Set the volume

Open `config.txt` on the card in a plain text editor and adjust:

```
VOLUME=1.0
```

`1.0` is normal, `0.0` is silent, `2.0` is as loud as it goes. Anything
higher is automatically reduced. The speaker is small — close whispers and
voices carry well; music and deep sounds do not.

Your `config.txt` is never overwritten by the Conformer, so you can set this
once and keep re-running it.

## Step 6 — Try it

Put the card in the scanner and power it on.

- The ghost appears on screen almost immediately, showing **WAKING UP**.
- After about ten seconds it changes to **PLACE GHOST HERE** with an arrow
  pointing down. **It cannot read ghosts until it says PLACE GHOST HERE.**
- Hold a ghost against the bottom of the unit, below the screen.
- The ghost appears and its audio plays. It clears itself when the audio
  finishes, or tap the screen to clear it early.

## How long should a clip be?

As long as you like — the scanner plays audio straight off the card, so
length costs nothing. Roughly 2.6 MB per minute.

The real trade-off is different: **while a ghost is playing, the scanner
ignores all other ghosts.** It starts listening again when the clip finishes
or someone taps the screen. So a three-minute clip means a guest who wanders
off leaves the prop unresponsive for up to three minutes. Shorter clips make
the prop feel livelier; longer ones are fine if you expect people to stay
and listen.

## If something goes wrong

| What you see | What it means | What to do |
|---|---|---|
| **NO SD CARD** | No card, or it can't be read | Reseat the card. Reformat as FAT32 and copy `2-READY-FOR-CARD` again. |
| **THAT'S NO SPIRIT** on every tag | Wrong type of tag | Check they are NTAG213/215/216, not MIFARE Classic. |
| **THAT'S NO SPIRIT** on one tag | The tag's text doesn't match a filename | Check the tag says exactly the filename, e.g. `ghost01`. |
| Ghost appears but is **silent** | Its sound didn't convert | Re-run the Conformer and check that file was listed as OK. |
| Ghost appears, no picture | No picture for that ghost | Normal if you didn't make one. |
| Screen says **WAKING UP** and stays there | Still starting | Wait ten seconds. If it never changes, the card may be unreadable. |
| Nothing happens at all | No ghosts loaded | Hold the screen for 5 seconds to see the status screen. |

### The hidden status screen

**Press and hold the screen for 5 seconds.** A status screen shows whether
the card is readable, how many ghosts loaded, and the volume. Tap once to
dismiss.

This is the fastest way to answer "is it the card or the tag?". If it says
`Ghosts: NONE LOADED`, the problem is the card. If it says `SD card:
ABSENT`, the card isn't being read at all.

A quick tap does nothing — deliberate, so guests can't open it by accident.

## Appendix — doing it by hand

You should not need this; the Conformer exists so you don't have to. But if
you ever want to prepare a file yourself, these are the exact requirements.
They are strict, and a file in the wrong format looks perfectly normal on
your computer and simply won't work.

**Sound** — WAV, **Signed 16-bit PCM**, **Mono**, **22050 Hz**.
In Audacity: **Tracks → Mix → Mix Stereo down to Mono**, then
**Tracks → Resample… → 22050**, then **File → Export Audio…** with Format
`WAV`, Encoding `Signed 16-bit PCM`. Menu wording shifts between Audacity
versions; those four values are what matter.

**Picture** — **24-bit uncompressed BMP**, exactly **240 wide × 320 tall**.
Not 32-bit, no compression, no run-length encoding. The width in particular
must be exactly 240.

**Names** — lowercase, no spaces, matching the tag text.

---

# Part 2 — Developer

## Relationship to `main`

This is the `ghost-event` branch of `arduino-cyd-player-scanner`. It shares
history with `main` (the ALN player scanner) so hardware fixes can be
cherry-picked in both directions, but it is not intended to merge.

The ALN parent repo stays pinned to `main`. Work happens in a git worktree
so the submodule checkout inside `ALN-Ecosystem` is never disturbed.

`main` is the orchestrator-connected ALN scanner. This branch removes all of
it: no WiFi, HTTP, TLS, queue, session, scoring or video. Flash usage drops
from 59% to 22% as a result.

See `docs/GHOST_EVENT_PLAN.md` for the full design record.

## Build and flash

Requires `arduino-cli` and the ESP32 core pinned at 3.3.2:

```bash
arduino-cli core install esp32:esp32@3.3.2 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Libraries are vendored in `libraries/` and `--libraries` pins the build to
them, so the result does not depend on what is installed on the machine:

```bash
# compile
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PartitionScheme=no_ota,UploadSpeed=921600 \
  --libraries ./libraries \
  ALNScanner_v5

# compile and upload
arduino-cli compile --upload -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=no_ota,UploadSpeed=921600 \
  --libraries ./libraries \
  ALNScanner_v5
```

## Tests

```bash
pio test -e native      # 44 cases across 4 suites, no hardware needed
```

CI runs the native suites and an `arduino-cli` cross-compile on every push.
The compile job exists because native tests are pure logic and never
cross-compile — that gap previously let a non-compiling change reach
hardware.

## Serial access

GPIO 3 is shared between Serial RX and the RFID chip select, so only one can
work at a time.

- `DEBUG_MODE=false` (normal): RFID initialises at boot, serial RX is dead.
- `DEBUG_MODE=true`: RFID is deferred, serial commands work. Send
  `START_SCANNER` to enable RFID, which kills serial RX for that boot.
- **Boot override**: send any character within 10 seconds of boot to force
  debug mode regardless of `config.txt`.

Opening the serial port resets the ESP32, so use one persistent session
rather than open/send/close per command.

Commands: `HELP`, `MEM`, `REBOOT`, `CONFIG`, `STATUS`, `TOKENS`,
`SET_CONFIG`, `SAVE_CONFIG`, `START_SCANNER`, `SIMULATE_SCAN`,
`SIMULATE_FAIL`.

`TOKENS` prints `FOUND`/`missing` per file for every ghost, which is the
quickest way to diagnose a card remotely. `SIMULATE_SCAN:ghost01` drives the
real presentation path — with no orchestrator there is no network step to
stub, so what it shows is what a real tap shows.

## The Conformer

`conformer/` holds the sources for the folder sent to whoever prepares a
card. `scripts/build-conformer-zip.sh` assembles it into a shippable zip,
fetching static ffmpeg builds for both platforms.

ffmpeg binaries are **not committed** (~150MB for both). The build script
caches them in `.conformer-cache/`, which is gitignored.

```bash
./scripts/build-conformer-zip.sh          # -> dist/Ghost Asset Conformer.zip
```

Two caveats the build script also prints:

- The macOS ffmpeg is an **x86_64** build, running on Apple Silicon only via
  Rosetta. Drop an arm64 binary at `.conformer-cache/ffmpeg-mac` and rebuild
  if that's a problem.
- **The Windows path has never been executed.** `conform.ps1` was written
  without a Windows machine or PowerShell available to test against. Run it
  once on real Windows before shipping.

`conform.sh` and `conform.ps1` are parallel implementations of the same
logic. If you change a format constant in one, change it in the other.

For your own use, `scripts/prepare-ghost-assets.sh <in> <out>` does the same
conversion directly with whatever ffmpeg is on PATH.

## Format constraints

These come from the decoder and renderer, not from preference:

- **Audio**: uncompressed PCM WAV, 8 or 16-bit, mono or stereo. No MP3
  decoder is compiled in. Output is the ESP32's internal 8-bit DAC.
- **Images**: 24-bit uncompressed BMP, `compression == 0`, positive height
  (bottom-up rows). Dimensions are *not* validated, but `rowBytes = width*3`
  has **no row-padding handling**, so width must be a multiple of 4 or every
  row desyncs. 240×320 satisfies all of it.
- **Tags**: NTAG21x only. `parseNDEFText()` rejects any card with
  `SAK != 0x00`, which excludes MIFARE Classic.

## Hardware notes

The display must initialise before the SD card (shared VSPI bus), and SD
reads must complete before locking the TFT or the bus deadlocks. The RFID
RX gain is deliberately set to 33dB (`RFCfgReg 0x40`), not the 48dB maximum
— the higher gain saturates the receiver at resting contact distance. Both
carry over from `main`; see `ALNScanner_v5/config.h` and `CLAUDE.md`.
