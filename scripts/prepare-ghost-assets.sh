#!/usr/bin/env bash
#
# prepare-ghost-assets.sh — conform source audio and images into the exact
# formats the ghost scanner can play and display.
#
# DEVELOPER CONVENIENCE ONLY. The documented route for whoever prepares a
# card is Audacity plus an image editor (see README.md), because they are on
# Windows or macOS without this toolchain. Use this when you have ffmpeg to
# hand and want to batch-convert.
#
# WHAT IT DOES: format only. It does not decide which ghost is which, and it
# does not edit tokens.json.
#
# An earlier version numbered outputs ghost01/ghost02/... in glob order,
# which assigned ghosts alphabetically by whatever the source files happened
# to be called. Every file converted fine, every name was valid, and the
# ghosts said the wrong things — silently wrong, with nothing to diagnose.
#
# Output names are derived from input names using the SAME rule the firmware
# applies to tag text (models/Token.h cleanTokenId): lowercase, spaces and
# colons removed. Name your sources ghost01.mp3 / ghost01.png and you get
# ghost01.wav / ghost01.bmp directly, with no renaming step.
#
# Usage:
#   ./scripts/prepare-ghost-assets.sh <input-dir> <output-dir> [fill|fit]
#
#   fill (default) scales to cover 240x320 and centre-crops the overflow.
#   fit            scales to fit inside 240x320 and pads with black.

set -euo pipefail

# --- audio: AudioGeneratorWAV takes uncompressed PCM WAV, 8/16-bit only ---
readonly SAMPLE_RATE=22050
readonly CHANNELS=1
readonly ACODEC=pcm_s16le

# --- image: DisplayDriver::parseBMPHeader + the render loop require ---
#   * 24bpp, compression == 0 (BI_RGB)          - both explicitly checked
#   * positive height (bottom-up rows)          - loop runs height-1 .. 0
#   * width * 3 divisible by 4                  - rowBytes has NO padding
#                                                 handling, so any other
#                                                 width desyncs every row
readonly IMG_W=240
readonly IMG_H=320
readonly PIXFMT=bgr24

usage() {
    echo "Usage: $0 <input-dir> <output-dir> [fill|fit]" >&2
    exit 2
}

[ $# -ge 2 ] && [ $# -le 3 ] || usage

readonly IN_DIR=$1
readonly OUT_DIR=$2
readonly MODE=${3:-fill}

case "$MODE" in
    fill|fit) ;;
    *) echo "error: mode must be 'fill' or 'fit', got '$MODE'" >&2; exit 2 ;;
esac

command -v ffmpeg >/dev/null 2>&1 || {
    echo "error: ffmpeg not found. Install it, or use the manual route in README.md." >&2
    exit 1
}

[ -d "$IN_DIR" ] || { echo "error: input dir not found: $IN_DIR" >&2; exit 1; }

readonly AUDIO_OUT="$OUT_DIR/assets/audio"
readonly IMAGE_OUT="$OUT_DIR/assets/images"
mkdir -p "$AUDIO_OUT" "$IMAGE_OUT"

if [ "$MODE" = "fill" ]; then
    readonly VF="scale=${IMG_W}:${IMG_H}:force_original_aspect_ratio=increase,crop=${IMG_W}:${IMG_H}"
else
    readonly VF="scale=${IMG_W}:${IMG_H}:force_original_aspect_ratio=decrease,pad=${IMG_W}:${IMG_H}:(ow-iw)/2:(oh-ih)/2:black"
fi

# Mirror cleanTokenId(): drop extension, lowercase, strip spaces and colons.
clean_name() {
    local base
    base=$(basename "$1")
    base=${base%.*}
    printf '%s' "$base" | tr '[:upper:]' '[:lower:]' | tr -d ' :'
}

names=()

convert_one() {
    local src=$1 kind=$2 out_dir=$3 ext=$4
    local clean out

    clean=$(clean_name "$src")
    if [ -z "$clean" ]; then
        echo "  skipping (name empty after cleaning): $(basename "$src")" >&2
        return
    fi

    out="$out_dir/$clean.$ext"

    # Refuse to clobber: two sources cleaning to one name (Ghost01.png and
    # "ghost 01.jpg") would otherwise leave whichever ran last, silently.
    if [ -e "$out" ]; then
        echo "  ERROR: $(basename "$out") already exists - two sources clean to the same name" >&2
        echo "         offending input: $(basename "$src")" >&2
        exit 1
    fi

    if [ "$kind" = audio ]; then
        ffmpeg -loglevel error -y -i "$src" \
            -acodec "$ACODEC" -ar "$SAMPLE_RATE" -ac "$CHANNELS" "$out"
    else
        ffmpeg -loglevel error -y -i "$src" \
            -vf "$VF" -pix_fmt "$PIXFMT" -frames:v 1 "$out"
    fi

    printf "  %-28s -> %-20s %s\n" "$(basename "$src")" "$clean.$ext" "$(du -h "$out" | cut -f1)"
    names+=("$clean")
}

shopt -s nullglob nocaseglob
audio_files=("$IN_DIR"/*.{wav,mp3,m4a,aac,flac,ogg,aif,aiff})
image_files=("$IN_DIR"/*.{png,jpg,jpeg,bmp,gif,tif,tiff,webp})
shopt -u nocaseglob

if [ ${#audio_files[@]} -eq 0 ] && [ ${#image_files[@]} -eq 0 ]; then
    echo "error: no audio or image files found in $IN_DIR" >&2
    exit 1
fi

if [ ${#audio_files[@]} -gt 0 ]; then
    echo "Audio -> ${SAMPLE_RATE}Hz mono 16-bit PCM WAV"
    for f in "${audio_files[@]}"; do convert_one "$f" audio "$AUDIO_OUT" wav; done
    echo
fi

if [ ${#image_files[@]} -gt 0 ]; then
    echo "Images -> ${IMG_W}x${IMG_H} 24-bit uncompressed BMP (${MODE})"
    for f in "${image_files[@]}"; do convert_one "$f" image "$IMAGE_OUT" bmp; done
    echo
fi

echo "Written to $OUT_DIR/assets/"
echo
echo "Copy assets/ onto the card. This script does NOT edit tokens.json;"
echo "paste the block below into it (see README.md Step 5):"
echo
{
    echo '{'
    echo '  "tokens": {'
    mapfile -t uniq < <(printf '%s\n' "${names[@]}" | sort -u)
    last=$(( ${#uniq[@]} - 1 ))
    for i in "${!uniq[@]}"; do
        if [ "$i" -eq "$last" ]; then
            printf '    "%s": {}\n' "${uniq[$i]}"
        else
            printf '    "%s": {},\n' "${uniq[$i]}"
        fi
    done
    echo '  }'
    echo '}'
} | sed 's/^/    /'
