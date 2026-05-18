---
phase: 06-multimodal
plan: 01
subsystem: audio
tags: [audio-capture, ring-buffer, atomics, processBlock, multimodal]

requires:
  - phase: 01-foundation
    provides: PluginProcessor base class, processBlock, audioBufferOut

provides:
  - RepentePd::AudioCapture — wait-free push/pull audio ring buffer
  - PluginProcessor::audioCapture public member
  - PluginProcessor::startAudioCapture(float durationSec)
  - CMakeLists glob for Source/RepentePd/*.cpp (auto-picks 06-02/03 sources)

affects: [06-02-spectral, 06-03-bridge-listen]

tech-stack:
  added: []
  patterns:
    - "Wait-free audio capture: armed/ready/writePos atomics; no mutex in feedAudio"
    - "Audio thread → message thread handoff via atomic state machine (armed → filling → ready → idle)"

key-files:
  created:
    - Source/RepentePd/AudioCapture.h
    - Source/RepentePd/AudioCapture.cpp
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - CMakeLists.txt

key-decisions:
  - "channels_ stored as atomic int (not passed to feedAudio) — audio thread reads from captureBuffer post-resize"
  - "startCapture disarms before resize, re-arms after (release fence) — prevents audio thread writing to buffer during setSize"
  - "CMakeLists glob covers RepentePd/*.cpp — future plans 06-02/03 auto-included without CMake edits"
  - "takeCapture uses makeCopyOf not std::move — captureBuffer may be mid-read by audio thread briefly after ready_ set"

patterns-established:
  - "feedAudio: arm gate → ready gate → atomic pos read → copy → pos update → check completion — all wait-free"

duration: ~20min
started: 2026-05-05T00:00:00Z
completed: 2026-05-05T00:00:00Z
---

# Phase 06 Plan 01: AudioCapture Summary

**Wait-free audio ring buffer (atomic state machine) tapped from processBlock; PluginProcessor exposes startAudioCapture() + audioCapture for multimodal Plans 06-02/03.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20min |
| Started | 2026-05-05 |
| Completed | 2026-05-05 |
| Tasks | 2/2 |
| Files modified | 5 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Capture accumulates correct duration | Pass | ceil(durationSec * sampleRate) samples; isReady() fires when writePos >= target |
| AC-2: takeCapture() returns buffer and resets | Pass | makeCopyOf + full state reset; returns empty buffer if not ready |
| AC-3: Audio thread never blocks | Pass | feedAudio uses only atomic loads/stores; no mutex anywhere |
| AC-4: Zero overhead when idle | Pass | Single `armed.load(acquire)` gate — returns immediately when not capturing |

## Accomplishments

- `AudioCapture.h/.cpp`: atomic state machine (armed → filling → ready → idle); feedAudio is 6 atomic ops + memcpy when active, 1 atomic load when idle
- `PluginProcessor`: `audioCapture.feedAudio(audioBufferOut)` tapped after `outputFifo->writeAudioAndMidi` in the main DSP block
- `CMakeLists.txt`: added `RepentePd/*.h` + `RepentePd/*.cpp` glob — Plans 06-02 and 06-03 source files are auto-picked

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1+2: all changes | `4c3ec6f7e` | feat | AudioCapture + PluginProcessor wiring + CMakeLists glob |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/AudioCapture.h` | Created | Class declaration + atomic members |
| `Source/RepentePd/AudioCapture.cpp` | Created | startCapture / feedAudio / takeCapture implementation |
| `Source/PluginProcessor.h` | Modified | `#include AudioCapture.h`; public `audioCapture` member + `startAudioCapture()` |
| `Source/PluginProcessor.cpp` | Modified | `audioCapture.feedAudio(audioBufferOut)` after outputFifo write |
| `CMakeLists.txt` | Modified | Added `RepentePd/*.h` + `RepentePd/*.cpp` glob lines |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| `startCapture` disarms before resize, re-arms after | Prevents audio thread writing to captureBuffer during `setSize()` which allocates | Safe audio-thread resize without locks |
| `takeCapture` uses `makeCopyOf` | After `ready_=true`, audio thread has exited feedAudio but captureBuffer still referenced | Avoids move-from-live-reference race |
| CMakeLists glob at `RepentePd/*.cpp` level | Plans 06-02 (SpectralAnalyzer) and 06-03 (Bridge) also land in RepentePd/ | Zero CMake friction for remaining plans |

## Deviations from Plan

None — plan executed exactly as written.

Plan noted "Avoid: calling feedAudio on the bypassed path" — investigation showed `processBlockBypassed` delegates to `processBlock(bypassBuffer)` and pd still fills `audioBufferOut`. Tap is correct on all paths; capturing during bypass yields pd output (which may be non-silent).

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- `PluginProcessor::audioCapture` accessible from PluginEditor/Bridge via `pd->audioCapture`
- `startAudioCapture(float durationSec)` callable from message thread
- `audioCapture.isReady()` / `audioCapture.takeCapture()` for Plan 06-03

**Concerns:**
- `takeCapture` uses `makeCopyOf` (allocation on message thread) — acceptable for /listen use case (not real-time)
- Default capture channels = `getMainBusNumOutputChannels()` — stereo in most setups; SpectralAnalyzer (06-02) should handle mono/stereo

**Blockers:**
- None

---
*Phase: 06-multimodal, Plan: 01*
*Completed: 2026-05-05*
