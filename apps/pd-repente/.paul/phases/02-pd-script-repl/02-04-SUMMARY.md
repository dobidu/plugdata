---
phase: 02-pd-script-repl
plan: 04
subsystem: ui
tags: [lua, luajit, pds, repl, help]

requires:
  - phase: 02-03
    provides: ObjectTreePanel, onRegistryChanged callback, Executor (create/connect/delete/move/list)

provides:
  - /help <topic> — 7-topic paged help (pds, sugar, lua, llm, commands, builtin, overview)
  - Executor::executeSync — synchronous execute on message thread for Lua callbacks
  - pds Lua table — pds.create/connect/delete/move/list callable from Lua blocks
  - Executor::pruneDeletedObjects — prune stale registry on GUI-side object removal
  - PluginEditor::refreshObjectsPanel — shared prune+refresh path, called on synchronise

affects: [03-repente-bridge, 04-bidirectionality]

tech-stack:
  added: [LuaJIT C API (lua_State, luaL_*), unordered_set]
  patterns:
    - "Lua closures as private static members — class access via lightuserdata upvalue"
    - "Canvas::performSynchronise as object-removal hook for REPL registry"

key-files:
  created: []
  modified:
    - Source/RepentePd/UI/PromptInput.cpp
    - Source/RepentePd/UI/PromptInput.h
    - Source/RepentePd/Core/Executor.h
    - Source/RepentePd/Core/Executor.cpp
    - Source/Sidebar/CommandInput.h
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - Source/Canvas.cpp

key-decisions:
  - "pds Lua closures as private static members of PromptInput (not file-scope) — need class-private access to executor/pluginEditor"
  - "pruneDeletedObjects called from Canvas::performSynchronise after object-removal loop — hooks into existing synchronise path"
  - "builtin topic added (not in original plan) — documents plugdata CommandInput commands"

patterns-established:
  - "registerLuaExtension(fn) on CommandInput — injection hook for pds or future tables"
  - "executeSync always on message thread; submit() for cross-thread dispatch"

duration: ~3h (over multiple sessions)
started: 2026-04-30T00:00:00Z
completed: 2026-05-01T00:00:00Z
---

# Phase 02 Plan 04: /help topics + pds Lua table

**Synchronous pds Lua table (pds.create/connect/delete/move/list), paged /help system, and ObjectTreePanel GUI-deletion sync — Phase 02 REPL complete.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~3h |
| Started | 2026-04-30 |
| Completed | 2026-05-01 |
| Tasks | 2 + 1 checkpoint |
| Files modified | 8 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: /help <topic> returns topic-specific text | Pass | 7 topics: overview, pds, sugar, lua, llm, commands, builtin |
| AC-2: pds table works from Lua — single object | Pass | pds.create returns name; pds.delete removes object |
| AC-3: pds loop creates multiple objects | Pass | Loop + connect verified by user |

## Accomplishments

- `pds` Lua global table registered on PromptInput construction; closures use lightuserdata upvalue for class access
- `Executor::executeSync()` enables synchronous pd-script calls from Lua (message thread only, jassert guarded)
- `/help` with 7 topics — overview + pds/sugar/lua/llm/commands/builtin; ASCII-only output avoids encoding artifacts
- ObjectTreePanel now syncs on GUI deletions: `pruneDeletedObjects()` + `refreshObjectsPanel()` hooked into `Canvas::performSynchronise`

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1+2: /help + pds table | `4eef17da6` | feat(02-04): /help <topic> + pds Lua table (synchronous) |
| Bug-fix: help encoding/truncation | `a776a6542` | fix(help): ASCII dashes, fix truncation, add builtin topic |
| Bug-fix: connect arg order + GUI sync | `1778ce55a` | Fix pds.connect arg order and refresh ObjectTreePanel on GUI deletions |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | /help topics; pds Lua closures as static members |
| `Source/RepentePd/UI/PromptInput.h` | Modified | lua_State fwd decl; private static closure decls |
| `Source/RepentePd/Core/Executor.h` | Modified | executeSync + pruneDeletedObjects declarations |
| `Source/RepentePd/Core/Executor.cpp` | Modified | executeSync + pruneDeletedObjects impl; Object.h + unordered_set include |
| `Source/Sidebar/CommandInput.h` | Modified | registerExtension on LuaExpressionParser; registerLuaExtension protected on CommandInput |
| `Source/PluginEditor.h` | Modified | refreshObjectsPanel() declaration |
| `Source/PluginEditor.cpp` | Modified | refreshObjectsPanel() impl; call from handleAsyncUpdate |
| `Source/Canvas.cpp` | Modified | Call refreshObjectsPanel() after object-removal loop in performSynchronise |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| pds closures as private static members | File-scope statics can't access class-private fields (executor, pluginEditor) | Pattern for any future Lua binding needing PromptInput access |
| pruneDeletedObjects from Canvas::performSynchronise | Canvas already iterates objects on every synchronise; cheapest hook point | GUI deletions, undo, external pd mutations all update panel |
| builtin topic added | User requested plugdata built-in commands documented in /help | Covers sel/ls/find/canvas/pd/script/reset/man/{}/> |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Bug-fixes (encoding, arg order) | 2 | Caught during verification — essential fixes |
| Scope addition (builtin topic) | 1 | User request during checkpoint |
| Scope addition (GUI deletion sync) | 1 | Discovered during checkpoint; ObjectTreePanel stale on GUI delete |

**Total impact:** All additions essential; no unrelated scope creep.

### Auto-fixed Issues

**1. Help text encoding artifacts**
- Found during: checkpoint verify
- Issue: Unicode em-dashes (—) rendered as â€" in plugdata console
- Fix: replaced all with ASCII ` --`
- Files: PromptInput.cpp
- Commit: `a776a6542`

**2. pds.connect wrong arg order**
- Found during: checkpoint verify (step 9)
- Issue: command built as `connect src outlet sink inlet` but Executor expects `connect src sink outlet inlet`
- Fix: swapped b/nout order in command string
- Files: PromptInput.cpp
- Commit: `1778ce55a`

**3. ObjectTreePanel stale on GUI deletions**
- Found during: checkpoint verify
- Issue: deleting objects via GUI didn't update panel; Executor registry held stale void* pointers
- Fix: pruneDeletedObjects() + refreshObjectsPanel() hooked into Canvas::performSynchronise
- Files: Executor.h, Executor.cpp, PluginEditor.h, PluginEditor.cpp, Canvas.cpp
- Commit: `1778ce55a`

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| protected: placement in CommandInput.h broke public methods | Moved registerLuaExtension to end of class after final public section |
| /help lua too long for single logMessage | Split into two logMessage calls |
| VDPAU linker error on Linux standalone | Pre-existing system library issue; unrelated to Phase 02 |

## Next Phase Readiness

**Ready:**
- Phase 02 REPL fully functional: create/connect/delete/move/list via /pds, sugar syntax, Lua loops, /help
- pds Lua table available for LLM-generated scripts (Phase 03)
- ObjectTreePanel live-synced with REPL registry + GUI mutations
- `refreshObjectsPanel()` available as shared path for Phase 03 bridge callbacks

**Concerns:**
- ObjectTreePanel shows REPL-named objects only (Phase 04 extends to all canvas objects)
- canvasStates stale on context-menu tab close — benign, will GC on canvas destroy
- pds table registered once at construction; canvas-switch does not re-register (by design)

**Blockers:** None

---
*Phase: 02-pd-script-repl, Plan: 04*
*Completed: 2026-05-01*
