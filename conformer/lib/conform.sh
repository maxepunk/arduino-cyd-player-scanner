#!/usr/bin/env bash
#
# conform.sh — shared core for the macOS launcher.
#
# Conforms whatever the user dropped into 1-PUT-YOUR-FILES-HERE into the
# exact formats the ghost scanner accepts, and writes a matching tokens.json
# so they never have to hand-edit JSON.
#
# Windows has an equivalent in conform.ps1. THE TWO MUST STAY IN STEP —
# if you change a format constant here, change it there.

set -uo pipefail

readonly IN_DIR="1-PUT-YOUR-FILES-HERE"
readonly OUT_DIR="2-READY-FOR-CARD"

# Audio: AudioGeneratorWAV accepts uncompressed PCM WAV, 8/16-bit only.
readonly SAMPLE_RATE=22050
readonly CHANNELS=1
readonly ACODEC=pcm_s16le

# Image: parseBMPHeader + the render loop require 24bpp, compression 0,
# positive height (bottom-up), and width*3 divisible by 4 because there is
# no row-padding handling. 240x320 satisfies all of it.
readonly IMG_W=240
readonly IMG_H=320
readonly PIXFMT=bgr24

FFMPEG="tools/mac/ffmpeg"
[ -x "$FFMPEG" ] || FFMPEG=$(command -v ffmpeg || true)

echo ""
echo "==============================================="
echo "   Ghost Asset Conformer"
echo "==============================================="
echo ""

if [ -z "$FFMPEG" ] || [ ! -x "$FFMPEG" ]; then
    echo "PROBLEM: the conversion tool is missing."
    echo ""
    echo "The folder 'tools/mac' should contain a file called 'ffmpeg'."
    echo "If you unzipped everything together it should be there."
    echo "Try downloading and unzipping the whole folder again."
    exit 1
fi

if [ ! -d "$IN_DIR" ]; then
    echo "PROBLEM: can't find the folder '$IN_DIR'."
    echo "This file needs to stay in the same folder as it."
    exit 1
fi

mkdir -p "$OUT_DIR/assets/audio" "$OUT_DIR/assets/images"

# Only clear DERIVED output. config.txt lives in $OUT_DIR and is the user's
# to edit — wiping the whole folder would silently discard their volume.
rm -f "$OUT_DIR/assets/audio/"*.wav "$OUT_DIR/assets/images/"*.bmp 2>/dev/null || true

# Mirror cleanTokenId(): drop extension, lowercase, strip spaces and colons.
clean_name() {
    local base
    base=$(basename "$1")
    base=${base%.*}
    printf '%s' "$base" | tr '[:upper:]' '[:lower:]' | tr -d ' :'
}

names=()
errors=0

shopt -s nullglob nocaseglob
audio_files=("$IN_DIR"/*.{wav,mp3,m4a,aac,flac,ogg,aif,aiff})
image_files=("$IN_DIR"/*.{png,jpg,jpeg,bmp,gif,tif,tiff,webp})
shopt -u nocaseglob

if [ ${#audio_files[@]} -eq 0 ] && [ ${#image_files[@]} -eq 0 ]; then
    echo "There are no files in '$IN_DIR' yet."
    echo ""
    echo "Put your ghost sounds (and pictures, if you have any) in there,"
    echo "then run this again."
    exit 1
fi

convert_one() {
    local src=$1 kind=$2 out_dir=$3 ext=$4
    local clean out

    clean=$(clean_name "$src")
    if [ -z "$clean" ]; then
        echo "  SKIPPED  $(basename "$src")  (name becomes empty)"
        errors=$((errors + 1))
        return
    fi

    out="$out_dir/$clean.$ext"
    if [ -e "$out" ]; then
        echo "  PROBLEM  $(basename "$src")"
        echo "           Another file is already called '$clean.$ext'."
        echo "           Two files can't share a ghost name - rename one."
        errors=$((errors + 1))
        return
    fi

    if [ "$kind" = audio ]; then
        "$FFMPEG" -loglevel error -y -i "$src" \
            -acodec "$ACODEC" -ar "$SAMPLE_RATE" -ac "$CHANNELS" "$out" 2>/dev/null
    else
        "$FFMPEG" -loglevel error -y -i "$src" \
            -vf "scale=${IMG_W}:${IMG_H}:force_original_aspect_ratio=increase,crop=${IMG_W}:${IMG_H}" \
            -pix_fmt "$PIXFMT" -frames:v 1 "$out" 2>/dev/null
    fi

    if [ $? -ne 0 ] || [ ! -s "$out" ]; then
        echo "  PROBLEM  $(basename "$src")  (could not be converted)"
        rm -f "$out"
        errors=$((errors + 1))
        return
    fi

    echo "  OK       $(basename "$src")  ->  $clean.$ext"
    names+=("$clean")
}

if [ ${#audio_files[@]} -gt 0 ]; then
    echo "Sounds:"
    for f in "${audio_files[@]}"; do convert_one "$f" audio "$OUT_DIR/assets/audio" wav; done
    echo ""
fi

if [ ${#image_files[@]} -gt 0 ]; then
    echo "Pictures:"
    for f in "${image_files[@]}"; do convert_one "$f" image "$OUT_DIR/assets/images" bmp; done
    echo ""
fi

if [ ${#names[@]} -eq 0 ]; then
    echo "Nothing was converted successfully. See the problems above."
    exit 1
fi

# Write tokens.json so nobody has to hand-edit JSON. A missing comma in a
# hand-edited file is the single most likely way to break a working card.
mapfile -t uniq < <(printf '%s\n' "${names[@]}" | sort -u)
{
    echo '{'
    echo '  "tokens": {'
    last=$(( ${#uniq[@]} - 1 ))
    for i in "${!uniq[@]}"; do
        if [ "$i" -eq "$last" ]; then printf '    "%s": {}\n' "${uniq[$i]}"
        else printf '    "%s": {},\n' "${uniq[$i]}"; fi
    done
    echo '  }'
    echo '}'
} > "$OUT_DIR/tokens.json"

echo "==============================================="
echo "  Done - ${#uniq[@]} ghost(s) ready"
echo "==============================================="
echo ""
printf '  '; printf '%s  ' "${uniq[@]}"; echo ""
echo ""
echo "Now copy EVERYTHING inside '$OUT_DIR'"
echo "onto your microSD card."
echo ""
if [ "$errors" -gt 0 ]; then
    echo "NOTE: $errors file(s) had problems - see above."
    echo ""
fi
