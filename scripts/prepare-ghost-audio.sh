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
# Usage:
#   ./scripts/prepare-ghost-audio.sh <input-dir> <output-dir>
#
# Every audio file in <input-dir> is converted and written to <output-dir>
# named ghostNN.wav in sorted order. Rename afterwards if your ghosts are
# not meant to be numbered in that order.

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

n=0
for f in "${files[@]}"; do
    n=$((n + 1))
    out=$(printf "%s/ghost%02d.wav" "$OUT_DIR" "$n")

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
echo "Remember the filenames must match the keys in tokens.json."
