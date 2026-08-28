#!/usr/bin/env bash
#
# prepare-ghost-audio.sh — convert source audio into the exact WAV format
# the ghost scanner can play.
#
# DEVELOPER CONVENIENCE ONLY. The documented route for whoever prepares a
# card is Audacity (see README.md), because they are on Windows or macOS
# without this toolchain. Use this when you have ffmpeg to hand and want to
# batch-convert.
#
# The scanner's decoder (AudioGeneratorWAV) accepts uncompressed PCM WAV at
# 8 or 16 bits, mono or stereo, only. It rejects everything else — MP3, and
# any WAV variant with a compressed or float encoding. Output here is
# 16-bit PCM / mono / 22050 Hz, which is the format the README publishes.
#
# This converts format ONLY. It does not decide which ghost is which.
#
# An earlier version numbered outputs ghost01/ghost02/... in glob order,
# which assigned ghosts alphabetically by whatever the source files happened
# to be called. Every file converted fine, every name was valid, and the
# ghosts said the wrong things — silently wrong, with nothing to diagnose.
#
# Output names are now derived from the input names using the SAME rule the
# firmware applies to tag text (models::Token.h cleanTokenId): lowercase,
# spaces and colons removed. So name your sources ghost01.mp3 and you get
# ghost01.wav directly, with no renaming step at all.
#
# Usage:
#   ./scripts/prepare-ghost-audio.sh <input-dir> <output-dir>

set -euo pipefail

readonly SAMPLE_RATE=22050
readonly CHANNELS=1
readonly CODEC=pcm_s16le      # signed 16-bit little-endian PCM

usage() {
    echo "Usage: $0 <input-dir> <output-dir>" >&2
    exit 2
}

[ $# -eq 2 ] || usage

readonly IN_DIR=$1
readonly OUT_DIR=$2

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "error: ffmpeg not found. Install it, or use the Audacity route in README.md." >&2
    exit 1
fi

[ -d "$IN_DIR" ] || { echo "error: input dir not found: $IN_DIR" >&2; exit 1; }
mkdir -p "$OUT_DIR"

shopt -s nullglob nocaseglob
files=("$IN_DIR"/*.{wav,mp3,m4a,aac,flac,ogg,aif,aiff})
shopt -u nocaseglob

if [ ${#files[@]} -eq 0 ]; then
    echo "error: no audio files found in $IN_DIR" >&2
    exit 1
fi

echo "Converting ${#files[@]} file(s) -> ${SAMPLE_RATE}Hz mono 16-bit PCM WAV"
echo

for f in "${files[@]}"; do
    # Mirror cleanTokenId(): strip the extension, lowercase, drop spaces
    # and colons. Matches what the firmware does to the text on a tag.
    base=$(basename "$f")
    base=${base%.*}
    clean=$(printf '%s' "$base" | tr '[:upper:]' '[:lower:]' | tr -d ' :')

    if [ -z "$clean" ]; then
        echo "  skipping (name empty after cleaning): $(basename "$f")" >&2
        continue
    fi

    out="$OUT_DIR/$clean.wav"

    # Refuse to clobber: two sources cleaning to one name (Ghost01.mp3 and
    # ghost 01.wav) would otherwise leave whichever ran last, silently.
    if [ -e "$out" ]; then
        echo "  ERROR: $(basename "$out") already exists - two sources clean to the same name" >&2
        echo "         offending input: $(basename "$f")" >&2
        exit 1
    fi

    ffmpeg -loglevel error -y -i "$f" \
        -acodec "$CODEC" -ar "$SAMPLE_RATE" -ac "$CHANNELS" \
        "$out"

    # Duration matters beyond file size: RFID is blocked for the whole clip,
    # so clip length is the prop's dead time after a guest walks away.
    dur=$(ffprobe -loglevel error -show_entries format=duration \
            -of default=noprint_wrappers=1:nokey=1 "$out" 2>/dev/null || echo "?")
    size=$(du -h "$out" | cut -f1)

    printf "  %-28s -> %-14s %5ss  %s\n" \
        "$(basename "$f")" "$(basename "$out")" "${dur%.*}" "$size"
done

echo
echo "Done. Copy these into the card's assets/audio/ folder."
echo
echo "Each filename above must have a matching entry in tokens.json."
echo "This script does NOT edit tokens.json - see README.md Step 5."
