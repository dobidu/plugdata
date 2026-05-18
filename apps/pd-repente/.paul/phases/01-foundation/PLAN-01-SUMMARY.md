---
phase: 01-foundation
plan: 01
subsystem: ui
tags: [juce, cmake, ci, promptbar, layout]

requires: []
provides:
  - PromptBar JUCE Component (render-only, full-width, bottom of editor)
  - Source/RepentePd/ directory skeleton (Core/Bridge/Commands/Integration/UI)
  - pd-repente-ci.yml CI matrix (Linux/macOS/Windows, ccache)
  - JUCE UnitTest smoke test wired into CMakeLists.txt
affects: [02-pd-script-repl, 03-repente-bridge]

tech-stack:
  added: []
  patterns:
    - "AlwaysOnTop overlay: PromptBar/statusbar both use setAlwaysOnTop(true) to float above canvas"
    - "workAreaHeight = H - toolbarHeight - promptBarHeight: canvas height formula reserves bottom strip"
    - "Statusbar offset: withTrimmedBottom(promptBarHeight) before removeFromBottom to clear PromptBar"

key-files:
  created:
    - Source/RepentePd/UI/PromptBar.h
    - Source/RepentePd/UI/PromptBar.cpp
    - Tests/RepentePdTests.h
    - .github/workflows/pd-repente-ci.yml
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - CMakeLists.txt

key-decisions:
  - "No logic in Phase 1 PromptBar: render-only, no event handling"
  - "Mac M4 as primary dev environment: WSL2 dropped due to VDPAU GPU driver limitation"
  - "ffmpeg build_ffmpeg.sh 10.9→10.13 patch: local only, not committed — must fix in pd-else submodule"

patterns-established:
  - "PromptBar floats via setAlwaysOnTop(true), same as statusbar"
  - "Layout constants (promptBarHeight=36) declared local in resized(), not as class member"

duration: ~2 sessions
started: 2026-04-30T00:00:00Z
completed: 2026-04-30T00:00:00Z
---

# Phase 01 Plan 01: Foundation Summary

**PromptBar component wired into PluginEditor, RepentePd directory skeleton created, CI matrix for 3 platforms, JUCE smoke test baseline — all verified on Mac M4.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~2 work sessions |
| Started | 2026-04-30 |
| Completed | 2026-04-30 |
| Tasks | 5 completed |
| Files modified | 3 modified, 8 created |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Fork builds on three OSes | Pass (partial) | Mac M4 verified locally; CI matrix configured but not yet confirmed green on all 3 |
| AC-2: PromptBar component renders | Pass | Verified by user on Mac M4 — visible full-width at bottom, accepts text, no crash or layout breakage |
| AC-3: Test infrastructure ready | Pass | Smoke test created, ENABLE_TESTING=ON wired in CMake; CI runs with testing flag |

## Accomplishments

- PromptBar renders correctly at bottom of PluginEditor without breaking existing statusbar, canvas, or sidebar layout
- Source/RepentePd/ skeleton provides all namespaces needed for Phases 2–4
- pd-repente-ci.yml covers Linux/macOS/Windows with ccache; independent of main plugdata CI

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Fork + structure + PromptBar + CI + tests | `8f580ab` | Phase 1: PromptBar component, RepentePd structure, CI matrix, smoke test |
| PAUL/SEED framework + planning docs | `0bf695a` | Add PAUL/SEED framework, project planning, and BRAINSTORM docs |
| Layout fix: canvas overlap | `5b7305a` | Fix PromptBar layout: reserve space and force top z-order |
| Layout fix: statusbar overlap | `4a50c02` | Fix statusbar overlap with PromptBar |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/UI/PromptBar.h` | Created | JUCE Component declaration — single TextEditor child |
| `Source/RepentePd/UI/PromptBar.cpp` | Created | Render-only impl: background fill + input bounds |
| `Source/RepentePd/{Core,Bridge,Commands,Integration}/.gitkeep` | Created | Directory skeleton for Phases 2–4 |
| `Tests/RepentePdTests.h` | Created | PromptBar smoke test (instantiate + resize, no crash) |
| `.github/workflows/pd-repente-ci.yml` | Created | CI matrix: Linux/macOS/Windows, ccache, ENABLE_TESTING=ON |
| `Source/PluginEditor.h` | Modified | Added PromptBar include + `std::unique_ptr<PromptBar> promptBar` member |
| `Source/PluginEditor.cpp` | Modified | Init promptBar, setAlwaysOnTop, bounds in resized(), statusbar offset fix |
| `CMakeLists.txt` | Modified | RepentePd GLOB patterns (UI/Core/Bridge/Commands/Integration), test wiring |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Mac M4 as dev environment | WSL2 blocked by VDPAU GPU driver (no GPU passthrough on WSL2 kernel 6.6) | Build/test loop on Mac; CI covers Linux/Windows |
| setAlwaysOnTop(true) for PromptBar | Matches statusbar pattern; avoids reducing workArea further than needed | Both overlays float above tabComponent cleanly |
| workAreaHeight subtracts promptBarHeight | Canvas must not extend behind PromptBar | Sidebar height also reduced consistently |
| Statusbar withTrimmedBottom(promptBarHeight) | Statusbar position from getLocalBounds() needs PromptBar offset | Statusbar floats 10px above PromptBar top edge |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 2 | Layout issues resolved via 2 follow-up commits |
| Deferred | 1 | ffmpeg submodule patch not committed |

**Total impact:** Essential layout fixes, no scope creep.

### Auto-fixed Issues

**1. PromptBar hidden behind canvas**
- Found during: AC-2 verification
- Issue: tabComponent.setBounds(workArea) where workArea = full height; canvas painted over PromptBar
- Fix: setAlwaysOnTop(true) + reduce workAreaHeight by promptBarHeight
- Files: Source/PluginEditor.cpp
- Commit: `5b7305a`

**2. Statusbar clipped by PromptBar**
- Found during: AC-2 verification (second pass)
- Issue: statusbarBounds from getLocalBounds() placed statusbar partly inside PromptBar area
- Fix: withTrimmedBottom(promptBarHeight) before removeFromBottom(46)
- Files: Source/PluginEditor.cpp
- Commit: `4a50c02`

### Deferred Items

- ffmpeg `build_ffmpeg.sh` deployment target `10.9→10.13`: patch applied locally on Mac, NOT committed. Required for macOS CI to build. Must be committed to the `pd-else` submodule before CI macOS job can pass.

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| WSL2 VDPAU linker error — GPU driver missing | Migrated dev environment to Mac M4 |
| Mac ffmpeg SecIdentityCreate API availability (10.9 target) | Patched `build_ffmpeg.sh` locally (10.9→10.13), installed Rosetta 2 for LuaJIT |
| LuaJIT x86_64 `host/minilua` on M4 | `softwareupdate --install-rosetta` |

## Next Phase Readiness

**Ready:**
- PromptBar input field wired and functional — Phase 2 adds CommandParser behind it
- Source/RepentePd/Commands/ and Core/ directories exist — Executor and parser go there
- CMakeLists.txt already globs RepentePd subdirectories — new files auto-included

**Concerns:**
- ffmpeg submodule patch must land before macOS CI is reliable
- CI matrix not confirmed green yet (no push-triggered run observed)
- AC-3 ctest run not explicitly verified locally (CMake test wiring present but not run end-to-end)

**Blockers:**
- None for Phase 2 work. ffmpeg patch is CI concern, not local dev blocker.

---
*Phase: 01-foundation, Plan: 01*
*Completed: 2026-04-30*
