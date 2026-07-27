#!/usr/bin/env bash
#
# Raise the macOS deployment target used for the bundled ffmpeg build.
#
# ffmpeg's libavformat/tls_securetransport.c calls SecIdentityCreate, which is
# annotated as macOS 10.12+. pd-else's build_ffmpeg.sh pins the ffmpeg build to
# -mmacosx-version-min=10.9 and compiles with -Werror, so on runner images whose
# SDK carries that annotation the build fails:
#
#   libavformat/tls_securetransport.c:168:16: error: 'SecIdentityCreate' is only
#   available on macOS 10.12 or newer [-Werror,-Wunguarded-availability]
#
# 10.13 regresses nothing: plugdata targets 10.15 normally and 10.11 for
# MACOS_LEGACY, both already above the 10.9 the script asks for.
#
# This lives here rather than in the submodule because Libraries/pd-else tracks
# upstream timothyschoen/pd-else, which we cannot push to. If the fix lands
# upstream, delete this script and its workflow steps.

set -euo pipefail

script="Libraries/pd-else/Source/Shared/ffmpeg/build_ffmpeg.sh"
from="version-min=10.9"
to="version-min=10.13"

if [ ! -f "$script" ]; then
    echo "::error::$script not found — did the submodule check out?"
    exit 1
fi

if grep -q -- "$to" "$script"; then
    echo "Already patched: $script pins $to"
    exit 0
fi

# Fail loudly instead of silently no-op'ing if upstream restructures the script.
if ! grep -q -- "$from" "$script"; then
    echo "::error::expected '$from' in $script but found neither it nor '$to'."
    echo "::error::Upstream pd-else likely changed the ffmpeg build; re-check whether this patch is still needed."
    grep -n "version-min" "$script" || echo "(no version-min flags at all)"
    exit 1
fi

# Portable in-place edit — BSD and GNU sed disagree on -i's argument.
tmp="$(mktemp)"
sed "s/${from}/${to}/g" "$script" > "$tmp"
mv "$tmp" "$script"
chmod +x "$script"

echo "Patched $script:"
grep -n "version-min" "$script"
