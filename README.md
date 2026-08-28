# Ghost Scanner

A standalone prop. A guest places a ghost on the scanner; the scanner
recognises it and plays that ghost's audio, showing a picture if one has
been provided.

That is the whole device. It does not connect to WiFi, a network, or any
server. Everything it knows comes from the microSD card inside it.

---

This README has two halves. **Part 1** is for whoever prepares the microSD
card and the ghosts — it assumes no programming and no special software
beyond one free audio editor. **Part 2** is for whoever builds and flashes
the firmware.

---

# Part 1 — Making a card

## What you need

| | |
|---|---|
| A microSD card | Any size. 8GB is far more than enough. |
| NFC tags | **NTAG213**, NTAG215 or NTAG216. See the warning below. |
| A phone | Android, or an iPhone that can write NFC tags. |
| A computer | Windows or macOS. |
| Audacity | Free, from audacityteam.org |

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

## Step 1 — Copy the template onto the card

1. Format the microSD card as **FAT32** (or **MS-DOS (FAT)** on a Mac).
2. Copy the **contents** of the `card-template` folder onto the card.

When you are done, the card should look like this — with `config.txt` and
`tokens.json` sitting at the top level, *not* inside a folder:

```
    (your SD card)
    ├── config.txt
    ├── tokens.json
    └── assets
        ├── audio
        └── images
```

## Step 2 — Prepare your audio

Every ghost needs one sound file, and it has to be in one specific format.
The scanner is fussy here and cannot be made less so: a file in the wrong
format looks perfectly normal on your computer and simply will not play.

**The format is: WAV, Signed 16-bit PCM, Mono, 22050 Hz.**

### In Audacity

1. Open your sound.
2. Make it **mono**, if it isn't already:
   **Tracks → Mix → Mix Stereo down to Mono**
3. Set the **sample rate** to 22050:
   **Tracks → Resample…** and choose `22050`
4. Export it:
   **File → Export Audio…** (older versions: **File → Export → Export as WAV**)

   In the export dialog, set:

   | Setting | Value |
   |---|---|
   | Format | **WAV** |
   | Encoding | **Signed 16-bit PCM** |
   | Channels | **Mono** |
   | Sample Rate | **22050** |

   Menu wording shifts a little between Audacity versions. If yours differs,
   the four values above are what matter — find them wherever they live.

5. Save it into the card's `assets/audio` folder.

### How long should a clip be?

As long as you like — the scanner streams audio off the card, so length
costs you nothing in storage. Roughly 2.5 MB per minute.

The real trade-off is different: **while a ghost is playing, the scanner
ignores all other ghosts.** It only starts listening again when the clip
finishes or someone taps the screen. So a three-minute clip means a guest
who wanders off leaves the prop unresponsive for up to three minutes.
Shorter clips make the prop feel livelier; longer clips are fine if you
expect people to stay and listen.

## Step 3 — Name the files

Each sound file must be named after its ghost:

```
    assets/audio/ghost01.wav
    assets/audio/ghost02.wav
    assets/audio/ghost03.wav
```

> ### ⚠️ Names must be lowercase, with no spaces
>
> `ghost01.wav` works. `Ghost01.wav` and `ghost 01.wav` do not.
>
> This trips people up because the **tag** is forgiving and the **file** is
> not. If you write `Ghost 01` onto a tag, the scanner tidies that up to
> `ghost01` and then goes looking for `ghost01.wav`. The tag's capitals and
> spaces are ignored; the filename's are not.
>
> When in doubt, keep everything lowercase everywhere.

## Step 4 — Pictures (optional)

Ghosts do not need pictures. Without one, the scanner draws its own ghost
on screen, which is the normal look for this build.

If you do want a picture for a ghost, it must be a **24-bit uncompressed
BMP, 240 wide × 320 tall**, named to match:

```
    assets/images/ghost01.bmp
```

Most image editors offer "24-bit" when saving as BMP. Do not use 32-bit,
and do not tick any compression option.

## Step 5 — Tell the card which ghosts exist

Open `tokens.json` on the card in a plain text editor (Notepad, TextEdit).
It arrives looking like this:

```json
{
  "tokens": {
    "ghost01": {},
    "ghost02": {},
    "ghost03": {},
    "ghost04": {},
    "ghost05": {}
  }
}
```

Add a line for every ghost you have, and delete any you don't. **Every line
needs a comma at the end except the last one** — that comma is the single
most common way to break this file.

Six ghosts:

```json
{
  "tokens": {
    "ghost01": {},
    "ghost02": {},
    "ghost03": {},
    "ghost04": {},
    "ghost05": {},
    "ghost06": {}
  }
}
```

The `{}` is meant to be empty. The scanner works out what each ghost has by
looking at the card, not by what this file claims.

## Step 6 — Write the tags

Install **NFC Tools** (free, Android and iOS).

For each ghost:

1. Open NFC Tools → **Write** → **Add a record**
2. Choose **Text**
3. Type the ghost's name exactly: `ghost01`
4. Tap **OK**, then **Write**, and hold the tag against your phone

Use **Text**, not "URL", not "Data". The scanner reads text records only.

To check a tag afterwards, use the **Read** tab in NFC Tools — it should
show the text you wrote and identify the tag as NTAG213 (or 215/216).

## Step 7 — Set the volume

Open `config.txt` on the card and adjust:

```
VOLUME=1.0
```

`1.0` is normal, `0.0` is silent, `2.0` is as loud as it goes. Anything
above `2.0` is automatically reduced. The speaker is small — expect close
whispers and voices to carry well, and music or deep sounds not to.

## Step 8 — Try it

Put the card in the scanner and power it on.

- After a few seconds you should see **PLACE GHOST HERE** with an arrow
  pointing down.
- Hold a ghost against the bottom of the unit, below the screen.
- The ghost appears and its audio plays. It clears itself when the audio
  finishes, or you can tap the screen to clear it early.

## If something goes wrong

| What you see | What it means | What to do |
|---|---|---|
| **NO SD CARD** | No card, or the scanner can't read it | Reseat the card. Reformat as FAT32 and copy the template again. |
| **THAT'S NO SPIRIT** on every tag | Wrong type of tag | Check they are NTAG213/215/216, not MIFARE Classic. |
| **THAT'S NO SPIRIT** on one tag | That ghost isn't in `tokens.json`, or its files are missing | Check the spelling matches exactly, all lowercase. |
| Ghost appears but is **silent** | The WAV is in the wrong format | Re-export from Audacity as Signed 16-bit PCM, mono, 22050 Hz. |
| Ghost appears, no picture | No BMP, or it isn't 24-bit | This is normal if you didn't add images. Otherwise re-save as 24-bit BMP. |
| Nothing happens at all | Card not readable, or no ghosts loaded | Hold the screen for 5 seconds to see the status screen. |
| Screen stays blank | Power | Check the USB cable and supply. |

### The hidden status screen

**Press and hold the screen for 5 seconds.** A status screen appears showing
whether the card is readable, how many ghosts loaded, and the volume. Tap
once to dismiss it.

This is the fastest way to answer "is it the card or the tag?". If it says
`Ghosts: NONE LOADED`, the problem is `tokens.json`. If it says `SD card:
ABSENT`, the card isn't being read at all.

A quick tap does nothing — that is deliberate, so guests can't open it by
accident.

---

# Part 2 — Developer

## Relationship to `main`

This is the `ghost-event` branch of `arduino-cyd-player-scanner`. It shares
history with `main` (the ALN player scanner) so hardware fixes can be
cherry-picked in both directions, but it is not intended to merge.

The ALN parent repo stays pinned to `main`. Work on this branch happens in
a git worktree so the submodule checkout inside `ALN-Ecosystem` is never
disturbed.

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

## Asset preparation

`scripts/prepare-ghost-audio.sh <in> <out>` batch-converts audio via ffmpeg
to the target format. A developer convenience only — the documented route
for whoever prepares a card is Audacity, because they are on Windows/macOS
without this toolchain.

**Scope: it replaces Part 1 Step 2 (format) and nothing else.** It does not
choose which ghost is which, and it does not touch `tokens.json`; Steps 4-7
still apply.

Output names are derived from input names using the same rule the firmware
applies to tag text (`cleanTokenId`): lowercase, spaces and colons removed.
So `Ghost 02.wav` becomes `ghost02.wav`, and naming your sources `ghost01.mp3`
means no renaming step at all. Two sources that clean to the same name are a
hard error rather than a silent overwrite.

An earlier version numbered outputs `ghost01`, `ghost02`… in glob order,
which assigned ghosts alphabetically by whatever the sources happened to be
called. Every file converted, every name was valid, and the ghosts said the
wrong things — worth remembering before adding "convenience" naming back.

## Format constraints

These come from the decoder and renderer, not from preference:

- **Audio**: uncompressed PCM WAV, 8 or 16-bit, mono or stereo. No MP3
  decoder is compiled in. Output is the ESP32's internal 8-bit DAC.
- **Images**: 24-bit uncompressed BMP. Dimensions are *not* validated —
  240×320 fills the portrait screen, smaller renders into the top-left,
  wider than 240 runs off.
- **Tags**: NTAG21x only. `parseNDEFText()` rejects any card with
  `SAK != 0x00`, which excludes MIFARE Classic.

## Hardware notes

The display must initialise before the SD card (shared VSPI bus), and SD
reads must complete before locking the TFT or the bus deadlocks. The RFID
RX gain is deliberately set to 33dB (`RFCfgReg 0x40`), not the 48dB maximum
— the higher gain saturates the receiver at resting contact distance. Both
constraints carry over from `main`; see `ALNScanner_v5/config.h` and
`CLAUDE.md`.
