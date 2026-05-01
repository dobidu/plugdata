---
phase: 02-pd-script-repl
plan: 02
subsystem: ui
tags: [executor, commandinput, promptinput, sugar, registry, canvas]

requires:
  - phase: 02-01
    provides: CommandParser, Executor stub, PromptBar wiring

provides:
  - Executor::execute() dispatching all 5 pd-script verbs to canvas
  - SugarExpander ($last + → arrow shorthand)
  - Per-canvas registry (CanvasState map — save/restore on tab switch)
  - PromptInput: unified CommandInput visual + /pds routing + free-text LLM stub
  - Sidebar CommandInput removed — single unified input bar

affects: 02-03 (RepenteClient wires into PromptInput free-text path), 02-04 (canvas serializer)

tech-stack:
  added: []
  patterns:
    - "Executor per-canvas CanvasState: save registry on tab switch, restore on return"
    - "PromptInput extends CommandInput: override executeCommand for /pds + free text routing"
    - "handleAsyncUpdate owns executor canvas sync — not onSubmit"

key-files:
  created:
    - Source/RepentePd/Commands/SugarExpander.h
    - Source/RepentePd/Commands/SugarExpander.cpp
    - Source/RepentePd/UI/PromptInput.h
    - Source/RepentePd/UI/PromptInput.cpp
  modified:
    - Source/RepentePd/Core/Executor.h
    - Source/RepentePd/Core/Executor.cpp
    - Source/Sidebar/CommandInput.h
    - Source/Sidebar/Sidebar.h
    - Source/Sidebar/Sidebar.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
  deleted:
    - Source/RepentePd/UI/PromptBar.h
    - Source/RepentePd/UI/PromptBar.cpp

key-decisions:
  - "PDS_MOVE: delta-based (getObjectBounds + moveObjects), not moveObjectTo — coordinate systems incompatible"
  - "Per-canvas CanvasState map: save/restore on setCanvas instead of clearing — tab switches preserve registry"
  - "setCanvas in handleAsyncUpdate only: removing per-submit call fixed spurious registry clears"
  - "PromptInput extends CommandInput (remove final, virtual executeCommand) — full visual + feature parity"
  - "Sidebar CommandInput removed entirely — PromptInput is sole input; selection sync via setConsoleTargetName"
  - "Free text → Repente LLM passthrough stub (Phase 03 wires real bridge)"

patterns-established:
  - "All /pds commands route through PromptInput::executeCommand → Executor::submit (async, result via pd->logMessage)"
  - "Arrow sugar: isArrow check before CommandParser, preArrowLast captured sync, connect submitted in async callback"
  - "Slash-prefix routing: /lua → base Lua, /anything → strip / + base CommandInput, free text → Repente"

duration: ~5 sessions
started: 2026-04-30T00:00:00Z
completed: 2026-05-01T00:00:00Z
---

# Phase 02 Plan 02: DirectCommands + SugarExpander + PromptInput Unification

**All 5 pd-script verbs execute real canvas mutations; PromptInput unifies the command bar — CommandInput visuals, /pds routing, and free-text LLM stub in one component.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~5 sessions |
| Started | 2026-04-30 |
| Completed | 2026-05-01 |
| Tasks | 2 planned + 3 auto-fix scope additions |
| Files modified | 11 |
| Commits | 8 (d622bf1 → 0f3f3bc) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: PDS_CREATE adds object to canvas | Pass | osc~, dac~, phasor~ all confirmed on Mac M4 |
| AC-2: PDS_CONNECT wires two objects | Pass | Outlet/inlet indices respected |
| AC-3: DELETE / MOVE / LIST work | Pass | MOVE needed delta fix (see deviations) |
| AC-4: $last + → arrow shorthand | Pass | Arrow creates + auto-connects in one gesture |
| Human verify checkpoint | Pass | User approved 2026-05-01 |

## Accomplishments

- `/pds create osc~` places object on canvas; `→ dac~` chains create + connect in one command
- Per-canvas registry: switching tabs preserves each tab's object namespace independently
- PromptInput replaces both the old PromptBar and the sidebar CommandInput — single unified input with CommandInput visual (rounded rect, `>` prefix, helper buttons, focus animation, history, Lua engine)
- Free-text input routes to Repente LLM stub (Phase 03 bridge slot ready)

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| DirectCommands + SugarExpander | `d622bf1` | Core REPL: create/connect/delete/move/list + $last + → |
| Debug output fix | `406d7d98` | stderr over DBG, canvas null-refresh |
| CommandParser raw fix | `b5773e26` | parsePds propagates raw field |
| Stale registry + MOVE coords | `d85fed91` | Tab-close stale fix (v1), delta-based MOVE |
| Registry clear timing | `8ac71b4f` | handleAsyncUpdate owns setCanvas; onSubmit null-guard only |
| PromptInput + per-canvas registry | `f5db5f2e` | Full CommandInput merge, CanvasState map |
| Sidebar unification | `0f3f3bc` | Remove sidebar CommandInput; single input bar |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Commands/SugarExpander.h/.cpp` | Created | $last + → expansion before parse |
| `Source/RepentePd/UI/PromptInput.h/.cpp` | Created | Unified input — extends CommandInput |
| `Source/RepentePd/UI/PromptBar.h/.cpp` | Deleted | Replaced by PromptInput |
| `Source/RepentePd/Core/Executor.h` | Modified | CanvasState struct + map; getCanvas() getter |
| `Source/RepentePd/Core/Executor.cpp` | Modified | All 5 verbs; per-canvas save/restore setCanvas |
| `Source/Sidebar/CommandInput.h` | Modified | Remove final; virtual executeCommand; virtual helper getters |
| `Source/Sidebar/Sidebar.h/.cpp` | Modified | Remove commandInput member; getCommandInputHeight → 0 |
| `Source/PluginEditor.h` | Modified | Forward-decl PromptInput; remove PromptBar |
| `Source/PluginEditor.cpp` | Modified | PromptInput init; handleAsyncUpdate setCanvas; setConsoleTargetName wiring |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Delta-based MOVE | `moveObjectTo` uses JUCE screen coords + magic offset; `createObject` uses pd coords — incompatible. `getObjectBounds` + `moveObjects(delta)` stays in pd coordinate space | MOVE works correctly |
| `setCanvas` in `handleAsyncUpdate` only | Per-submit call caused spurious registry clears because `getCurrentCanvas()` pointer was not stable across message-loop iterations | Registry no longer clears between commands in same tab |
| CanvasState map (save/restore) | Original design cleared registry on tab switch — each tab must maintain independent object namespace | Multi-tab workflows preserved correctly |
| Remove `final` from CommandInput, virtual `executeCommand` | Subclassing CommandInput is cleaner than duplicating 800+ lines of visual/interaction code | PromptInput reuses full CommandInput feature set |
| Sidebar CommandInput removed | User confirmed single unified input is the desired UX; sidebar input was redundant after PromptInput adoption | Sidebar console panel now takes full height |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 4 | Essential correctness fixes |
| Scope additions | 2 | User-approved during verify |
| Deferred | 0 | None |

**Total impact:** Scope additions were user-requested; auto-fixes were necessary for correctness. No plan tasks missed.

### Auto-fixed Issues

**1. PDS_MOVE coordinate system mismatch**
- Found during: Task 1 (DirectCommands)
- Issue: `moveObjectTo(x,y)` applies JUCE screen coords with +1542 magic offset; `createObject` uses pd pixel coords — objects moved to wrong positions
- Fix: Use `Interface::getObjectBounds` to read current te_xpix, then `moveObjects(delta)` to stay in pd coordinate space
- Commit: `d85fed91`

**2. DBG() invisible on macOS**
- Found during: Task 1 (testing)
- Issue: DBG() routes through NSLog → Apple unified logging; not visible in terminal
- Fix: Replaced all debug output with `fprintf(stderr, ...)`
- Commit: `406d7d98`

**3. setCanvas clearing registry on every other command**
- Found during: human-verify
- Issue: `getCurrentCanvas()` returns pointer from `SafePointer<Canvas>` which is not stable between message-loop iterations; per-submit setCanvas triggered spurious clears
- Fix: Moved `setCanvas` to `handleAsyncUpdate` (fires on real canvas-state changes only); onSubmit only initializes if canvas is null
- Commit: `8ac71b4f`

**4. CommandParser raw field empty for /pds**
- Found during: Task 1 (testing)
- Issue: `parsePds()` created new CommandResult without copying `raw` field
- Fix: Added `r.raw = trimmed` in `parse()` after `parsePds()` returns
- Commit: `b5773e26`

### Scope Additions

**1. Per-canvas registry (CanvasState map)**
- User request during verify: switching tabs lost all objects from previous tab
- Added `std::map<Canvas*, CanvasState>` to Executor — saves/restores {registry, nextId} on tab switch
- Commit: `f5db5f2e`

**2. PromptInput: full CommandInput visual + unification**
- User request during verify: "integrate (mix, using its visuals) our prompt bar with the command bar on the right side"
- Replaced PromptBar with PromptInput extending CommandInput; removed sidebar CommandInput; wired setConsoleTargetName to canvas selection events
- Commits: `f5db5f2e`, `0f3f3bc`

## Next Phase Readiness

**Ready:**
- Executor is fully functional for all 5 pd-script verbs + sugar
- PromptInput at bottom of main canvas handles all input routing
- Free-text path in PromptInput::executeCommand is the Phase 03 entry point — no wiring needed, just replace the passthrough stub
- Canvas selection updates PromptInput prefix (`osc~ >`) — ready for object-message workflows

**Concerns:**
- PromptInput sits at fixed 36px height; CommandInput's expand-on-focus animation will overlap canvas slightly when helper buttons appear — acceptable for now, improve layout in 02-03 or 02-04
- `canvasStates` map holds raw `Canvas*` keys — stale entries accumulate when tabs close (memory is small: string + void* pairs). Clean up on tab close in a future plan.
- Sidebar `setCommandTarget()` is now a no-op stub — callers in PluginEditor call `promptInput->setConsoleTargetName()` directly; stub can be removed later

**Blockers:** None

---
*Phase: 02-pd-script-repl, Plan: 02*
*Completed: 2026-05-01*
