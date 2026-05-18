---
phase: 05-polish
plan: 01
subsystem: ui
tags: [executor, autoplace, objectgrid, patchmerger, settings, unicode]

requires:
  - phase: 04-bidirectionality
    provides: PatchMerger merge pipeline, Executor registry, SettingsFile keys

provides:
  - Per-canvas cursor auto-placement in Executor
  - ObjectGrid snap after programmatic create
  - PatchMerger respects repente_autoplace setting
  - /config autoplace on|off toggle
  - fromUTF8 fix for all help/config console strings

affects: [05-02-arrange, 05-03-tree]

tech-stack:
  added: []
  patterns:
    - "Explicit coords bypass cursor (hasExplicitCoords guard in PDS_CREATE)"
    - "CanvasState saves/restores placementCursor on tab switch"
    - "juce::String::fromUTF8() for all strings with multi-byte escape sequences"

key-files:
  created: []
  modified:
    - Source/RepentePd/Core/Executor.h
    - Source/RepentePd/Core/Executor.cpp
    - Source/RepentePd/Bridge/PatchMerger.cpp
    - Source/RepentePd/UI/PromptInput.cpp
    - Source/Utility/SettingsFile.h

key-decisions:
  - "autoplace=off passes LLM coords as explicit, not just falls back to (100,100)"
  - "Cursor not advanced when explicit coords given (AC-2)"
  - "Unicode fix applied immediately — String(const char*) doesn't guarantee UTF-8 in this JUCE config"

patterns-established:
  - "String::fromUTF8() required for any string literal containing \\xNN multi-byte sequences"
  - "SettingsFile keys for new features must be in defaultSettings before use"

duration: ~2h
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 05 Plan 01: CanvasLayouter Auto-Placement Summary

**Per-canvas cursor auto-placement with ObjectGrid snap + `repente_autoplace` toggle; PatchMerger honors the flag; all console Unicode strings fixed.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~2h |
| Started | 2026-05-02 |
| Completed | 2026-05-02 |
| Tasks | 2 + 1 checkpoint completed |
| Files modified | 5 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: No-coord creates cascade cleanly | Pass | 90px step right, wrap at x>700→next row, ObjectGrid snap applied |
| AC-2: Explicit coordinates still respected | Pass | `hasExplicitCoords = (cmd.args.size() > 2)` bypasses cursor entirely |
| AC-3: PatchMerger merge mode uses auto-placement | Pass | Reads `repente_autoplace`; strips LLM coords when on |

## Accomplishments

- `Executor::CanvasState` gains `placementCursor` — per-canvas, saved/restored on tab switch
- `nextAutoPosition()` advances 90px right, wraps at x>700 → +60px row, reset x=50
- After `synchronise()`, iterates `canvas->objects` to get last `Object*`, calls `canvas->objectGrid.positionNewObject()`
- `repente_autoplace` SettingsFile key (default `true`) controls both Executor and PatchMerger
- When `autoplace=off`: REPL creates land at (100,100); PatchMerger passes LLM coords as explicit
- `/config autoplace on|off` command in PromptInput exposes the toggle live
- All help/config console strings wrapped in `juce::String::fromUTF8()` — no more `â` artifacts

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1+2: Cursor + PatchMerger | `33cd7d347` | feat | CanvasLayouter cursor auto-placement + ObjectGrid snap |
| Scope: autoplace toggle | `aab8b9e05` | feat | /config autoplace on|off |
| Auto-fix: Unicode | `351b0c5a5` | fix | String::fromUTF8 for all multi-byte strings |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Core/Executor.h` | Modified | Added `placementCursor` to CanvasState + member + `nextAutoPosition()` decl |
| `Source/RepentePd/Core/Executor.cpp` | Modified | Implemented `nextAutoPosition()`, cursor save/restore, PDS_CREATE wired |
| `Source/RepentePd/Bridge/PatchMerger.cpp` | Modified | Reads `repente_autoplace`; omits or passes LLM coords accordingly |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | `/config autoplace on|off` handler + `fromUTF8()` on all help/config strings |
| `Source/Utility/SettingsFile.h` | Modified | Added `repente_autoplace` default (`true`) to `defaultSettings` |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| `autoplace=off` uses LLM coords as explicit, not (100,100) | User request — preserves LLM layout intent when disabled | Objects respect model's spatial intent when autoplace off |
| Cursor not advanced for explicit-coord creates | AC-2 requirement — explicit placement must not disturb auto sequence | Mixing `/pds create osc~` and `/pds create osc~ 200 300` works predictably |
| `fromUTF8()` on all `\xNN` literals | `String(const char*)` in this JUCE config interprets bytes as Latin-1 | UTF-8 box-drawing and arrow chars display correctly in console |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Scope additions | 2 | Positive — user-requested feature + config robustness |
| Auto-fixed | 1 | Unicode display bug found during review |
| Deferred | 0 | — |

### Scope Additions

**1. `/config autoplace on|off` toggle**
- User asked for live toggle after auto-placement was working
- Added SettingsFile key, PromptInput handler, and Executor/PatchMerger reads

**2. `autoplace=off` passes LLM coords explicitly**
- Plan said "strip coords" only; user wanted to honor LLM coords when off
- PatchMerger now conditionally passes `llmX llmY` to `/pds create`

### Auto-fixed Issues

**1. Unicode corruption in console help/config strings**
- Found during review — `→`, `─`, `•`, `—` showed as `â`
- Root cause: `juce::String(const char*)` doesn't guarantee UTF-8 decode
- Fix: wrapped all `\xNN` literal strings in `juce::String::fromUTF8()`
- Commit: `351b0c5a5`

## Next Phase Readiness

**Ready:**
- Auto-placement stable foundation for 05-02 topology-aware arrange
- `repente_autoplace` toggle available for 05-02 to build on
- Executor `nextAutoPosition()` can be reset/overridden by future arrange pass

**Concerns:**
- Cursor doesn't reset between distinct "sessions" (no `/pds reset` command) — deferred to 05-04 or out of scope
- `canvasStates` stale entries on context-menu tab close — benign, GC on canvas destroy (open item)

**Blockers:**
- None

---
*Phase: 05-polish, Plan: 01*
*Completed: 2026-05-02*
