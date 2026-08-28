#!/usr/bin/env bash
#
# macOS launcher. Double-click this file.
#
# A double-clicked .command starts in the user's home folder, not here, so
# the first job is always to move to our own directory.

cd "$(dirname "$0")" || exit 1

# Downloaded files carry a quarantine flag and macOS refuses to run them.
# The bundled ffmpeg would be blocked on first use with an error a
# non-technical person cannot act on. Strip it, quietly; failure is fine
# because the flag may simply not be present.
xattr -dr com.apple.quarantine . >/dev/null 2>&1 || true
chmod +x lib/conform.sh tools/mac/ffmpeg >/dev/null 2>&1 || true

bash lib/conform.sh
status=$?

echo ""
echo "Press Return to close this window."
read -r _
exit $status
