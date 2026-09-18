#!/usr/bin/env bash
#
# download_remaining_1000.sh
#
# Downloads the remaining Gehring & Homberger 1000-customer instances.
#
# C1_10_1.TXT is already in the repo; the other 59 instances (C1, C2, R1, R2,
# RC1, RC2 — 10 per class) are fetched as a single archive from the official
# SINTEF mirror and unpacked into this same folder. Existing files are never
# overwritten, so the script is safe to re-run.
#
# Source: https://www.sintef.no/projectweb/top/vrptw/1000-customers/

set -euo pipefail

# The script's directory is also the instances directory.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

ZIP_URL="https://www.sintef.no/globalassets/project/top/vrptw/homberger/1000/homberger_1000_customer_instances.zip"

# Expected full set of 1000-customer instances (6 classes × 10).
declare -a EXPECTED=()
for cls in C1 C2 R1 R2 RC1 RC2; do
    for n in 1 2 3 4 5 6 7 8 9 10; do
        EXPECTED+=("${cls}_10_${n}.TXT")
    done
done

echo "Destination directory: $DIR"
echo "Source: $ZIP_URL"
echo

# Temporary file for the archive.
ZIP_FILE="$(mktemp -t homberger_1000.XXXXXX.zip)"
trap 'rm -f "$ZIP_FILE"' EXIT

echo "Downloading archive..."
if ! curl -fsSL "$ZIP_URL" -o "$ZIP_FILE"; then
    echo "Error: failed to download the archive. Check your network connection and URL." >&2
    exit 1
fi

if ! unzip -t "$ZIP_FILE" >/dev/null 2>&1; then
    echo "Error: downloaded file is not a valid zip archive." >&2
    exit 1
fi

echo "Extracting missing instances..."
extracted=0
skipped=0
missing_after=()
for name in "${EXPECTED[@]}"; do
    if [[ -f "$name" ]]; then
        skipped=$((skipped + 1))
        continue
    fi
    # Extract only this file (-o overwrite is safe since we verified absence).
    if unzip -o "$ZIP_FILE" "$name" >/dev/null 2>&1; then
        echo "  + $name"
        extracted=$((extracted + 1))
    else
        echo "  ! failed to extract $name" >&2
        missing_after+=("$name")
    fi
done

echo
echo "Done: added $extracted, already present $skipped, expected total ${#EXPECTED[@]}."

# Final integrity check of the set.
for name in "${EXPECTED[@]}"; do
    if [[ ! -f "$name" ]]; then
        missing_after+=("$name")
    fi
done

if (( ${#missing_after[@]} > 0 )); then
    echo "Warning: ${#missing_after[@]} file(s) missing:" >&2
    printf '  - %s\n' "${missing_after[@]}" >&2
    exit 1
fi

echo "1000-customer instance set complete: all ${#EXPECTED[@]} files present."
