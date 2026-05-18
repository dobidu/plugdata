---
phase: 06-multimodal
plan: 03
subsystem: bridge
tags: [listen, audio-context, llm-injection, spectral, multimodal, juce-timer]

requires:
  - phase: 06-multimodal/06-01
    provides: AudioCapture::startCapture() / takeCapture() — raw audio buffer
  - phase: 06-multimodal/06-02
    provides: SpectralAnalyzer::analyze() / format() — spectral text string

provides:
  - Bridge::send() with optional audioContext param → appended to system message
  - /listen [prompt] command — capture 3s audio → analyze → inject → LLM
  - Full C3 Tríade multimodal loop wired end-to-end

affects: [phase-07-onwards]

tech-stack:
  added: []
  patterns:
    - "juce::Timer::callAfterDelay for async capture wait — not MessageManager"
    - "audioContext appended to canvas context in single system message (newline-separated)"
    - "Raw pointer captures in lambda (Bridge*, PluginProcessor*) — safe for 3.2s delay"

key-files:
  modified:
    - Source/RepentePd/Bridge/Bridge.h
    - Source/RepentePd/Bridge/Bridge.cpp
    - Source/RepentePd/UI/PromptInput.cpp

key-decisions:
  - "audioContext appended to canvas context (not a separate system message) — one system message, simpler"
  - "juce::Timer::callAfterDelay not juce::MessageManager::callAfterDelay — correct JUCE API"
  - "getSampleRate() from PluginProcessor (AudioProcessor method) not from AudioCapture"
  - "3.2s delay (vs 3s capture) — 200ms margin for audio thread to flush final frame"

patterns-established:
  - "/listen [prompt] → captures then sends; empty prompt → default observer message"
  - "Canvas + spectral context both present in every /listen request"

duration: ~15min
started: 2026-05-05T00:00:00Z
completed: 2026-05-05T00:00:00Z
---

# Phase 06 Plan 03: Bridge + /listen Summary

**`/listen` command wires AudioCapture → SpectralAnalyzer → Bridge into a complete multimodal loop: capture 3s of audio output, analyze spectrum, inject both canvas patch and spectral summary as LLM system context.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~15min |
| Started | 2026-05-05 |
| Completed | 2026-05-05 |
| Tasks | 2/2 |
| Files modified | 3 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Bridge.send() accepts audioContext | Pass | 4th param `juce::String const& audioContext = {}`; appended to canvas context in system message |
| AC-2: /listen captures and sends | Pass | startAudioCapture(3.0f) + Timer::callAfterDelay(3200) + analyze + send |
| AC-3: /listen with no prompt | Pass | Default: "I just heard the audio output. What do you observe and suggest?" |
| AC-4: /listen with silent buffer | Pass | analyze() returns zero Result; bridge->send still called; no crash |
| AC-5: /listen when bridge null | Pass | "repente: bridge not ready -- use /config url to set server" |

## Accomplishments

- `Bridge::send()` extended with optional `audioContext` param — backward-compatible, all existing callers unchanged
- `/listen [prompt]` command: captures 3s audio → spectral analysis → injects into LLM system context alongside canvas
- C3 Tríade complete: generate → play → `/listen` → LLM sees both patch structure and audio spectrum

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1: Bridge audioContext | — | feat | Bridge.send() 4th param + injection |
| Task 2: PromptInput /listen | — | feat | /listen command + help text |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Bridge/Bridge.h` | Modified | Added `audioContext` param to send() declaration |
| `Source/RepentePd/Bridge/Bridge.cpp` | Modified | Updated signature; append audioContext to context string |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | Added SpectralAnalyzer include; /listen handler; /help llm + commands updated |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| audioContext appended to canvas context | One system message is simpler; LLM gets full context in single block | /listen sends canvas + spectral together |
| `juce::Timer::callAfterDelay` | Correct JUCE API; `MessageManager::callAfterDelay` doesn't exist | Fixed compile error during qualify |
| `pd->getSampleRate()` not `audioCapture.getSampleRate()` | AudioCapture doesn't store sampleRate; PluginProcessor inherits getSampleRate() from AudioProcessor | No new AudioCapture API needed |

## Deviations from Plan

### Auto-fixed Issues

**1. Wrong JUCE delay API**
- Found during: Task 2 qualify
- Issue: Plan specified `juce::MessageManager::callAfterDelay` — method doesn't exist on MessageManager
- Fix: `juce::Timer::callAfterDelay(3200, ...)` — correct static method
- No semantic change — same behavior

### Deferred Items
None.

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| `juce::MessageManager::callAfterDelay` compile error | Corrected to `juce::Timer::callAfterDelay` — standard JUCE pattern |

## Next Phase Readiness

**Ready:**
- C3 Tríade multimodal loop fully wired: `/listen [prompt]` operational end-to-end
- Phase 06 complete (3/3 plans)
- Milestone 3 (Multimodal Loop) feature-complete

**Concerns:**
- Lambda captures raw `Bridge*` and `PluginProcessor*` — safe for 3.2s delay within plugin session, but could dangle if plugin is destroyed while timer pending (edge case on rapid quit)
- No progress indicator during 3s capture window (console log only)

**Blockers:**
- None

---
*Phase: 06-multimodal, Plan: 03*
*Completed: 2026-05-05*
