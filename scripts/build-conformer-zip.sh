#!/usr/bin/env bash
#
# build-conformer-zip.sh — assemble the shippable "Ghost Asset Conformer"
# folder and zip it.
#
# Run this on the dev machine, then send the zip to whoever is preparing the
# card. They unzip it, drop files in, and double-click. No install.
#
# ffmpeg binaries are NOT committed to this repo (they are ~150MB for both
# platforms). This script fetches them, or reuses ones you have already
# placed in the cache directory.
#
# Usage:
#   ./scripts/build-conformer-zip.sh [output-dir]

set -euo pipefail

readonly REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
readonly OUT_ROOT=${1:-$REPO_ROOT/dist}
readonly STAGE="$OUT_ROOT/Ghost Asset Conformer"
readonly CACHE="$REPO_ROOT/.conformer-cache"

# Static builds. Both are widely used and self-contained.
#   Windows : gyan.dev  (x86_64)
#   macOS   : evermeet.cx (x86_64 — runs on Apple Silicon via Rosetta)
readonly WIN_URL="https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
readonly MAC_URL="https://evermeet.cx/ffmpeg/getrelease/zip"

log() { printf '  %s\n' "$*"; }

for tool in curl unzip zip; do
    command -v "$tool" >/dev/null 2>&1 || { echo "error: $tool required" >&2; exit 1; }
done

mkdir -p "$CACHE"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib" "$STAGE/tools/mac" "$STAGE/tools/win" \
         "$STAGE/1-PUT-YOUR-FILES-HERE" "$STAGE/2-READY-FOR-CARD"

echo "Assembling conformer..."

# --- scripts and docs ---
cp "$REPO_ROOT/conformer/READ ME FIRST.txt"        "$STAGE/"
cp "$REPO_ROOT/conformer/CONFORM MY FILES.command" "$STAGE/"
cp "$REPO_ROOT/conformer/CONFORM MY FILES.bat"     "$STAGE/"
cp "$REPO_ROOT/conformer/lib/conform.sh"           "$STAGE/lib/"
cp "$REPO_ROOT/conformer/lib/conform.ps1"          "$STAGE/lib/"
chmod +x "$STAGE/CONFORM MY FILES.command" "$STAGE/lib/conform.sh"
log "scripts + READ ME FIRST"

# config.txt ships in the OUTPUT folder: the conformer never overwrites it,
# so the volume they set survives every re-run.
cp "$REPO_ROOT/card-template/config.txt" "$STAGE/2-READY-FOR-CARD/"
log "config.txt -> 2-READY-FOR-CARD (conformer never touches it)"

# --- ffmpeg: Windows ---
if [ ! -f "$CACHE/ffmpeg.exe" ]; then
    log "fetching Windows ffmpeg..."
    curl -fsSL "$WIN_URL" -o "$CACHE/win.zip"
    unzip -qo "$CACHE/win.zip" -d "$CACHE/win-x"
    find "$CACHE/win-x" -name ffmpeg.exe -exec cp {} "$CACHE/ffmpeg.exe" \;
    rm -rf "$CACHE/win-x" "$CACHE/win.zip"
fi
cp "$CACHE/ffmpeg.exe" "$STAGE/tools/win/"
log "ffmpeg.exe ($(du -h "$CACHE/ffmpeg.exe" | cut -f1))"

# --- ffmpeg: macOS ---
if [ ! -f "$CACHE/ffmpeg-mac" ]; then
    log "fetching macOS ffmpeg..."
    curl -fsSL "$MAC_URL" -o "$CACHE/mac.zip"
    unzip -qo "$CACHE/mac.zip" -d "$CACHE/mac-x"
    find "$CACHE/mac-x" -name ffmpeg -type f -exec cp {} "$CACHE/ffmpeg-mac" \;
    rm -rf "$CACHE/mac-x" "$CACHE/mac.zip"
fi
cp "$CACHE/ffmpeg-mac" "$STAGE/tools/mac/ffmpeg"
chmod +x "$STAGE/tools/mac/ffmpeg"
log "ffmpeg (mac) ($(du -h "$CACHE/ffmpeg-mac" | cut -f1))"

# Finder hides empty folders badly and zip drops them entirely; a visible
# placeholder guarantees the input folder survives the round trip.
cat > "$STAGE/1-PUT-YOUR-FILES-HERE/put your sounds and pictures in this folder.txt" <<'EOF'
Put your ghost sounds here, named after each ghost:

    ghost01.mp3
    ghost02.wav

Pictures are optional. If you want one, give it the same name:

    ghost01.png

Then go back up and double-click CONFORM MY FILES.

You can delete this note.
EOF

cd "$OUT_ROOT"
rm -f "Ghost Asset Conformer.zip"
zip -qr "Ghost Asset Conformer.zip" "Ghost Asset Conformer"

echo ""
echo "Built: $OUT_ROOT/Ghost Asset Conformer.zip  ($(du -h "$OUT_ROOT/Ghost Asset Conformer.zip" | cut -f1))"
echo ""
echo "CHECK BEFORE SENDING:"
echo "  * The macOS ffmpeg is an x86_64 build. It runs on Apple Silicon"
echo "    only if Rosetta is installed. If they have an M-series Mac and"
echo "    it fails, swap in an arm64 build at .conformer-cache/ffmpeg-mac"
echo "    and re-run this."
echo "  * The Windows path has NOT been tested on Windows from this repo."
echo "    Run it once on a real Windows machine before shipping."
