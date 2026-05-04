---
phase: 05-polish
plan: 04
subsystem: ui
tags: [ollama, auto-detect, first-launch, privacy, onboarding, config]

requires:
  - phase: 03-repente-bridge
    provides: RepenteClient.ping(), SettingsFile config persistence, /config command
  - phase: 05-polish/05-01
    provides: PromptInput /config handler pattern

provides:
  - Ollama auto-detect on first launch (localhost:11434 → localhost:7860 → instructions)
  - Privacy warning when non-localhost URL is configured
  - /config display: backend type hint (Ollama / repente server / ⚠ remote)
  - /help llm: Ollama + OpenAI + repente server setup examples

affects: [v1.0-release, post-v1-backlog]

tech-stack:
  added: []
  patterns:
    - "Auto-detect chain: sequential async pings via lambda capture of this; fires on message thread"
    - "Privacy warning: local detection via startsWithIgnoreCase on http://localhost + 127.0.0.1"

key-files:
  modified:
    - Source/PluginEditor.cpp
    - Source/RepentePd/UI/PromptInput.cpp

key-decisions:
  - "Console-only wizard (no modal dialog) — consistent with pd-repente UX style"
  - "Default to llama3.2 when Ollama detected — most common model; user overrides with /config model"
  - "Sequential ping chain (not parallel) — avoids race condition if both servers happen to respond"

patterns-established:
  - "Auto-detect fires only when !sf->hasProperty('repente_url') — never overrides returning user config"

duration: ~30min
started: 2026-05-04T00:00:00Z
completed: 2026-05-04T00:00:00Z
---

# Phase 05 Plan 04: V1.0 Polish Summary

**First-launch Ollama auto-detect (localhost:11434 → 7860 → instructions), privacy warning on remote URLs, backend type hint in /config, and Ollama/OpenAI/repente setup examples in /help llm.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30min |
| Started | 2026-05-04 |
| Completed | 2026-05-04 |
| Tasks | 2 + 1 checkpoint |
| Files modified | 2 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Ollama auto-detected on first launch | Pass | Pings localhost:11434; saves url+model to SettingsFile on success |
| AC-2: Repente server fallback | Pass | Falls back to localhost:7860; shows instructions if neither found |
| AC-3: Privacy warning on remote URL | Pass | ⚠ message logged for non-localhost URLs; localhost = silent |
| AC-4: Help text updated for Ollama | Pass | /help llm shows Ollama/OpenAI/repente sections; /config shows (Ollama)/(⚠ remote) |

## Accomplishments

- `PluginEditor.cpp`: sequential async ping chain — Ollama first, repente server fallback, helpful instructions if neither; only fires on first launch (no `repente_url` in SettingsFile)
- `PromptInput.cpp /config url`: privacy warning for non-localhost URLs using `startsWithIgnoreCase`
- `PromptInput.cpp /config` display: URL backend hint `(Ollama)` / `(repente server)` / `(⚠ remote)`
- `/help llm`: replaced vague "Default: localhost:7860" with structured Ollama/OpenAI/repente examples

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1+2: all changes | `f60dd53fc` | feat | First-launch Ollama auto-detect + privacy warning + help update |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/PluginEditor.cpp` | Modified | Auto-detect block after config load; pings 11434→7860→instructions |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | Privacy warning in /config url; URL hint in /config display; /help llm update |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Console-only wizard, no modal | pd-repente is console-first; dialogs break UX consistency | Simpler, consistent with other first-run messages |
| Default model `llama3.2` for Ollama | Most commonly installed Ollama model | User needs to run `/config model <name>` if they use a different model |
| Sequential pings (not parallel) | Avoids race condition if both servers respond; simpler state machine | Adds ~5s max latency on first launch with no server running |

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- V1.0 roadmap 100% complete — all phases done
- Ollama users have zero-config onboarding
- Remote API users get informed consent via privacy warning

**Concerns:**
- `llama3.2` default may not be installed; user sees error on first LLM call → guided by `/config model` and `ollama list` instructions in auto-detect message
- Windows/Linux smoke tests not done in this plan — manual testing deferred

**Blockers:**
- None — V1.0 shippable

---
*Phase: 05-polish, Plan: 04*
*Completed: 2026-05-04*
