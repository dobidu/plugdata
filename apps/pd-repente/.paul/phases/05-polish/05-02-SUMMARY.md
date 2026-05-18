---
phase: 05-polish
plan: 02
subsystem: ui
tags: [arrange, topology, dag, bfs, executor, commandparser]

requires:
  - phase: 05-polish/05-01
    provides: Executor canvas mutation patterns, patch.moveObjects delta API

provides:
  - ArrangeEngine: BFS DAG topology layout for all canvas objects
  - /arrange command: PromptInput → CommandParser → Executor → ArrangeEngine
  - CommandType::PDS_ARRANGE

affects: [05-03-tree, 05-04-polish]

tech-stack:
  added: []
  patterns:
    - "Delta-based move: getObjectBounds → moveObjects(delta) — NOT moveObjectTo (has +1542 offset)"
    - "ArrangeEngine as static utility class — no state, pure canvas mutation"

key-files:
  created:
    - Source/RepentePd/Core/ArrangeEngine.h
    - Source/RepentePd/Core/ArrangeEngine.cpp
  modified:
    - Source/RepentePd/Commands/CommandParser.h
    - Source/RepentePd/Commands/CommandParser.cpp
    - Source/RepentePd/Core/Executor.cpp
    - Source/RepentePd/UI/PromptInput.cpp

key-decisions:
  - "Delta move via getObjectBounds + moveObjects — moveObjectTo adds +1542 offset (wrong coord space)"
  - "Pure topology depth (no type classification) — no-incoming = source, sufficient for signal flow"
  - "Isolated/cycle objects park in max_depth+1 column, not scattered"

patterns-established:
  - "NEVER use patch.moveObjectTo for programmatic repositioning — use getObjectBounds + patch.moveObjects"
  - "pd::Interface::getObjectBounds returns raw pd coords; moveObjectTo(x,y) maps to raw pd (x+1542, y+1542)"

duration: ~1h
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 05 Plan 02: Signal-Flow Arrange Summary

**`/arrange` command: BFS DAG topology assigns depth per object, repositions all canvas objects left-to-right in 130px columns with 70px vertical stacking; isolated objects park in max_depth+1 column.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~1h |
| Started | 2026-05-02 |
| Completed | 2026-05-02 |
| Tasks | 2 + 1 checkpoint completed |
| Files modified | 6 (2 created) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Signal chain arranged left-to-right | Pass | osc~→gain~→dac~ confirmed leftmost→middle→right |
| AC-2: Isolated objects in rightmost column | Pass | Disconnected objects park at max_depth+1 |
| AC-3: Empty/single-object canvas no crash | Pass | Returns "arrange: nothing to arrange" |

## Accomplishments

- `ArrangeEngine::arrange(Canvas*)` — BFS from sources (no incoming connections), assigns depth, groups into `std::map<int, vector>`, moves via delta
- `CommandType::PDS_ARRANGE` added to enum; `/arrange` parsed before generic `/...` catch
- `Executor::execute()` PDS_ARRANGE case delegates to `ArrangeEngine::arrange`
- PromptInput routes `/arrange` identical to `/pds` path; `/help commands` updated
- Coordinate space bug discovered and fixed in same session (auto-fix)

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1+2: ArrangeEngine + wiring | `2ef8c7f6d` | feat | ArrangeEngine BFS layout + CommandParser/Executor/PromptInput wiring |
| Auto-fix: coord space | `a818e5fe1` | fix | Delta-based move instead of moveObjectTo (+1542 offset) |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Core/ArrangeEngine.h` | Created | Static `arrange(Canvas*)` declaration |
| `Source/RepentePd/Core/ArrangeEngine.cpp` | Created | BFS topology + delta-based layout pass |
| `Source/RepentePd/Commands/CommandParser.h` | Modified | Added `PDS_ARRANGE` to CommandType enum |
| `Source/RepentePd/Commands/CommandParser.cpp` | Modified | `/arrange` detection before generic `/...` catch |
| `Source/RepentePd/Core/Executor.cpp` | Modified | PDS_ARRANGE case + ArrangeEngine.h include |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | `/arrange` routing + `/help commands` entry |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Pure topology depth (no type table) | Sufficient for signal flow; avoids maintaining type classification table | Simpler, works for any pd object |
| Isolated objects at max_depth+1 | Keeps them visible without mixing into topology columns | Clean visual separation |
| ArrangeEngine as static class, not integrated into Executor | Keeps topology logic out of command dispatch; easier to extend | Can be called from other places (e.g., post-merge auto-arrange) |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 1 | Critical — objects would land ~1542px below viewport |
| Scope additions | 0 | — |
| Deferred | 0 | — |

**Total impact:** One essential coordinate-space fix; no scope creep.

### Auto-fixed Issues

**1. Coordinate space mismatch in moveObjectTo**
- **Found during:** Checkpoint verification (user reported objects landed in bottom corner)
- **Issue:** `Patch::moveObjectTo(x, y)` calls `Interface::moveObject(x+1542, y+1542)` — the +1542 shifts to a different internal coord space than `createObject` uses
- **Fix:** Replace `moveObjectTo` with `getObjectBounds` to get current raw pd coords, then `patch.moveObjects({gobj}, targetX-curX, targetY-curY)` — same pattern as PDS_MOVE in Executor.cpp
- **Files:** `Source/RepentePd/Core/ArrangeEngine.cpp`
- **Verification:** User confirmed correct layout after fix
- **Commit:** `a818e5fe1`

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| Objects landed ~1542px below visible canvas after /arrange | Switched to delta-based move; root cause: moveObjectTo adds +1542 offset |

## Next Phase Readiness

**Ready:**
- ArrangeEngine available for post-merge auto-arrange (05-04 polish candidate)
- `/arrange` in help system; users can call after any LLM generation
- Delta-move pattern documented for future canvas manipulation code

**Concerns:**
- No undo support for /arrange — deferred to 05-04 polish
- Large patches (50+ objects) may need smarter column width than fixed 130px

**Blockers:**
- None

---
*Phase: 05-polish, Plan: 02*
*Completed: 2026-05-02*
