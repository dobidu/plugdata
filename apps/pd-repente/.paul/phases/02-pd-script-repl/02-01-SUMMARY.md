---
phase: 02-pd-script-repl
plan: 01
subsystem: repl
tags: [commandparser, executor, juce, messagethread, promptbar]

requires:
  - phase: 01-foundation
    provides: PromptBar component, RepentePd directory structure, CI matrix

provides:
  - CommandParser: stateless tokenizer for /pds grammar + PASSTHROUGH
  - Executor: thread-safe MessageManager::callAsync dispatcher + object registry stub
  - End-to-end pipeline: PromptBar Enter → parse → dispatch → stderr log

affects: 02-02 (DirectCommands consume CommandResult), 02-03 (OutputArea displays onResult strings)

tech-stack:
  added: []
  patterns:
    - "All canvas mutations dispatched via Executor::submit → callAsync (never direct)"
    - "All user input flows through CommandParser before reaching Executor"
    - "Use fprintf(stderr) not DBG() for terminal-visible output on macOS"

key-files:
  created:
    - Source/RepentePd/Commands/CommandParser.h
    - Source/RepentePd/Commands/CommandParser.cpp
    - Source/RepentePd/Core/Executor.h
    - Source/RepentePd/Core/Executor.cpp
  modified:
    - Source/RepentePd/UI/PromptBar.h
    - Source/RepentePd/UI/PromptBar.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - Tests/RepentePdTests.h

key-decisions:
  - "Canvas stored as raw pointer (not SafePointer) — refreshed on every submit via setCanvas()"
  - "Object registry uses void* to avoid libpd C headers in Executor.h"
  - "fprintf(stderr) instead of DBG() for macOS terminal output"

patterns-established:
  - "Executor::execute guarded by jassert(isThisTheMessageThread())"
  - "CommandParser::parsePds must copy raw field from outer CommandResult"

duration: ~3h (including debug sessions)
started: 2026-04-30T00:00:00Z
completed: 2026-04-30T00:00:00Z
---

# Phase 02 Plan 01: REPL Engine Summary

**CommandParser + Executor engine built and wired end-to-end: PromptBar Enter fires /pds tokenizer → thread-safe Executor dispatcher → stderr confirmation, all three command types exercised.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~3h |
| Tasks | 3 completed + 2 auto-fixes |
| Files created | 4 |
| Files modified | 5 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: CommandParser tokenizes pd-script input | Pass | `/pds create osc~ 100 100` → PDS_CREATE, args=[osc~,100,100]; PASSTHROUGH and UNKNOWN verified |
| AC-2: Executor dispatches on message thread | Pass | `jassert(isThisTheMessageThread())` + callAsync; no assertion fired |
| AC-3: End-to-end pipeline, no crash | Pass | All three types confirmed in terminal; PromptBar clears; no crash |

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1+2+3 (initial) | `d5a3d1ef7` | CommandParser, Executor, PromptBar wiring |
| Fix: canvas null + stderr | `406d7d98d` | Canvas refreshed on submit; fprintf(stderr) replaces DBG() |
| Fix: raw field propagation | `b5773e263` | parsePds() was not copying raw from outer CommandResult |

Plan metadata: `78e739a6f`

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Commands/CommandParser.h` | Created | CommandType enum, CommandResult struct, parser interface |
| `Source/RepentePd/Commands/CommandParser.cpp` | Created | parse() + parsePds() implementation |
| `Source/RepentePd/Core/Executor.h` | Created | Thread-safe dispatcher interface + object registry |
| `Source/RepentePd/Core/Executor.cpp` | Created | callAsync dispatch, stderr log stub, assignName/resolve |
| `Source/RepentePd/UI/PromptBar.h` | Modified | Added `onSubmit` callback |
| `Source/RepentePd/UI/PromptBar.cpp` | Modified | `input.onReturnKey` fires onSubmit, clears field |
| `Source/PluginEditor.h` | Modified | Added Executor.h include + `executor` member |
| `Source/PluginEditor.cpp` | Modified | Executor init + onSubmit lambda wiring |
| `Tests/RepentePdTests.h` | Modified | CommandParserTest: 7 cases covering all CommandTypes |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 2 | Essential correctness fixes |
| Deferred | 0 | — |

**Total impact:** Two bugs discovered during checkpoint verification, both fixed in-session.

### Auto-fixed Issues

**1. Canvas null at construction time**
- **Found during:** checkpoint:human-verify (no output appeared)
- **Issue:** Executor initialized with `getCurrentCanvas()` which returns nullptr before any patch opens; canvas never updated thereafter
- **Fix:** Init with nullptr; call `setCanvas(getCurrentCanvas())` on every submit
- **Commit:** `406d7d98d`

**2. DBG() not visible in macOS terminal**
- **Found during:** checkpoint:human-verify
- **Issue:** JUCE `DBG()` routes through NSLog → Apple unified logging; not visible in terminal when running binary directly
- **Fix:** Replace with `fprintf(stderr, ...)`
- **Commit:** `406d7d98d`

**3. CommandParser::parsePds not copying raw field**
- **Found during:** checkpoint output review (`RepentePd: not implemented: ` empty)
- **Issue:** `parsePds()` creates a new `CommandResult` without copying `raw` from the caller
- **Fix:** Assign `r.raw = trimmed` after `parsePds()` returns
- **Commit:** `b5773e263`

### Plan vs Actual — Minor Divergences

| Plan spec | Actual |
|-----------|--------|
| `juce::SafePointer<Canvas>` | Raw `Canvas*` refreshed on each submit via `setCanvas()` — SafePointer would pull in Canvas.h header in Executor.h, coupling the headers |
| `std::unordered_map<juce::String, t_gobj*>` | `std::map<juce::String, void*>` — avoids pd C headers in Executor.h; cast at call site in 02-02 |
| `JUCE_ASSERT_MESSAGE_THREAD` macro | `jassert(MessageManager::getInstance()->isThisTheMessageThread())` — same runtime effect |

## Next Phase Readiness

**Ready:**
- CommandResult flows correctly through full pipeline
- Executor thread invariant established and verified
- Object registry API (assignName/resolve) stubbed and ready for 02-02
- onResult callback pattern works end-to-end

**Concerns:**
- Canvas pointer refresh on every submit is safe but not as robust as SafePointer; 02-02 should call `setCanvas()` on patch open/close events via `TabComponent` listener
- `fprintf(stderr)` is debug-only; 02-03 OutputArea will replace it as the user-visible output channel

**Blockers:** None

---
*Phase: 02-pd-script-repl, Plan: 01*
*Completed: 2026-04-30*
