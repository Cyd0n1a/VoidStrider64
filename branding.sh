#!/usr/bin/env bash
#
# prepend-branding.sh
# Recursively prepend the contents of a branding file to the very top of
# every .c and .h file under ./src (relative to the current directory).
#
# SAFETY: The target is the *relative* path ./src, and find is never told
# to follow symlinks. As a result nothing above the current directory can
# be reached or modified -- even a symlink inside ./src that points upward
# is not descended into, and a source file that is itself a symlink is not
# matched by -type f, so it is never written through.

set -euo pipefail

# --- Configuration -----------------------------------------------------------
BRANDING="./branding.md"   # file whose contents get prepended
TARGET_DIR="./src"         # only this dir and its subdirectories are touched
# -----------------------------------------------------------------------------

# Best-effort copy of permission bits (GNU chmod, GNU stat, then BSD stat).
copy_mode() {
    local src="$1" dst="$2" mode
    chmod --reference="$src" "$dst" 2>/dev/null && return 0
    if mode=$(stat -c '%a' "$src" 2>/dev/null); then
        chmod "$mode" "$dst" 2>/dev/null && return 0
    fi
    if mode=$(stat -f '%Lp' "$src" 2>/dev/null); then
        chmod "$mode" "$dst" 2>/dev/null && return 0
    fi
    return 0
}

# Sanity checks
[ -f "$BRANDING" ]   || { echo "Error: branding file not found: $BRANDING" >&2; exit 1; }
[ -d "$TARGET_DIR" ] || { echo "Error: target directory not found: $TARGET_DIR" >&2; exit 1; }

brand_bytes=$(wc -c < "$BRANDING")

count=0
skipped=0

# -print0 + read -d '' => filenames with spaces/newlines are handled safely.
while IFS= read -r -d '' file; do

    # Idempotency: if the file already begins with the exact branding block,
    # leave it alone so re-running the script does not stack duplicates.
    if head -c "$brand_bytes" "$file" | cmp -s - "$BRANDING"; then
        printf 'skip (already branded): %s\n' "$file"
        skipped=$((skipped + 1))
        continue
    fi

    # Assemble in a temp file *in the same directory* so the final mv is
    # atomic and never crosses a filesystem boundary.
    tmp=$(mktemp "${file}.brand.XXXXXX")
    copy_mode "$file" "$tmp"

    # branding, then a newline separator, then the original content.
    { cat "$BRANDING"; printf '\n'; cat "$file"; } > "$tmp"

    mv "$tmp" "$file"
    printf 'branded: %s\n' "$file"
    count=$((count + 1))

done < <(find "$TARGET_DIR" -type f \( -name '*.c' -o -name '*.h' \) -print0)

printf '\nDone. %d file(s) branded, %d skipped.\n' "$count" "$skipped"
