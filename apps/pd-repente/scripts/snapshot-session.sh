#!/usr/bin/env bash
#
# Drive plugdata + pd-repente and capture one PNG per prompt. macOS only —
# it leans on screencapture and osascript, both of which ship with the OS.
#
#   ./snapshot-session.sh [outdir]
#
# First run will trigger two macOS permission prompts. Grant both to the
# terminal you launch this from, then re-run:
#   System Settings → Privacy & Security → Screen & System Audio Recording
#   System Settings → Privacy & Security → Accessibility
# Without Accessibility the keystrokes silently go nowhere; without Screen
# Recording the PNGs come out as empty desktop.
#
# The window is pinned to a fixed rect so every frame in the set is identical
# and screencapture -R can be deterministic. Don't move the window mid-run.

set -euo pipefail

OUTDIR="${1:-snapshots}"
APP="${PLUGDATA_APP:-plugdata}"        # app name as macOS knows it
X=0 Y=0 W=1600 H=1000                  # >=1600 wide for publication figures
GEN_WAIT="${GEN_WAIT:-25}"             # seconds to let a generation finish
MODEL="${MODEL:-repente:v0.3-pd}"

mkdir -p "$OUTDIR"

osa() { osascript "$@"; }

focus() { osa -e "tell application \"$APP\" to activate"; sleep 1; }

pin_window() {
  osa -e "tell application \"System Events\" to tell process \"$APP\"
            set position of window 1 to {$X, $Y}
            set size of window 1 to {$W, $H}
          end tell" 2>/dev/null || {
    echo "!! could not pin the window — is $APP running, and is Accessibility granted?" >&2
    exit 1
  }
  sleep 1
}

# Type into the prompt bar and hit return. The prompt bar has focus at startup;
# if you click elsewhere between shots, focus it again before calling this.
send() {
  osa -e "tell application \"System Events\" to keystroke \"$1\""
  sleep 0.4
  osa -e 'tell application "System Events" to key code 36'   # Return
}

shoot() {
  local name="$1"
  screencapture -x -R"$X,$Y,$W,$H" "$OUTDIR/$name.png"
  echo "  -> $OUTDIR/$name.png"
}

# ---------------------------------------------------------------------------

echo "== focusing + pinning window to ${W}x${H}"
focus
pin_window

echo "== configuring"
for c in \
  "/config url http://localhost:11434" \
  "/config model $MODEL" \
  "/config maxtokens 1536" \
  "/config autoplace on" \
  "/config test"
do
  send "$c"; sleep 2
done
shoot "00-config"

# Warm-up: first request pays model load. Never capture this one.
echo "== warm-up (not captured)"
send "create a sine wave at 440 Hz connected to dac~"
sleep "$GEN_WAIT"
send "/history clear"; sleep 1

# Battery B — the whitepaper's five Pd-generation tests. Reproducible figures.
echo "== Battery B"
i=1
while IFS= read -r p; do
  [ -z "$p" ] && continue
  printf '  B%d: %s\n' "$i" "$p"
  send "$p"
  sleep "$GEN_WAIT"
  shoot "$(printf 'b%02d-generated' "$i")"
  send "/arrange top-down 80"; sleep 3
  shoot "$(printf 'b%02d-arranged' "$i")"
  send "/history clear"; sleep 1
  i=$((i+1))
done <<'PROMPTS'
create a sine wave at 440 Hz connected to dac~
noise~ through a lop~ lowpass into dac~
osc~ with an LFO modulating amplitude into dac~
metro 500 driving a counter and sel for a rhythm pattern
osc~ into delwrite~ and delread~ for a delay line
PROMPTS

# Graceful degradation: no patch in the response. Prints the reason + the text
# instead of feeding prose to the Lua engine.
echo "== no-patch path"
send "/analyze what does this patch do?"
sleep "$GEN_WAIT"
shoot "90-analyze"

# Truncation: PatchFragment. Small cap forces a headerless #X body.
send "/config maxtokens 200"; sleep 1
send "osc~ into delwrite~ and delread~ for a delay line"
sleep "$GEN_WAIT"
shoot "91-truncated-fragment"
send "/config maxtokens 1536"; sleep 1

# Multimodal C3 — needs real audio, which is why this runs on macOS and not WSL.
echo "== /listen (needs audio)"
send "create a sine wave at 440 Hz connected to dac~"
sleep "$GEN_WAIT"
send "/listen"
sleep 12
shoot "92-listen-spectral"

echo
echo "done. $(ls -1 "$OUTDIR"/*.png 2>/dev/null | wc -l | tr -d ' ') frames in $OUTDIR/"
echo "Review every frame by hand — model output varies run to run, and a figure"
echo "set is only as good as its worst frame."
