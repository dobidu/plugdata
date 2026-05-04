---
phase: 05-polish
plan: 03
subsystem: ui
tags: [object-tree, selection, canvas, bidirectional-sync, juce, changelistener]

requires:
  - phase: 05-polish/05-01
    provides: Canvas mutation patterns, Object* API
  - phase: 05-polish/05-02
    provides: rowObjects structure baseline

provides:
  - ObjectTreePanel: clickable rows → canvas select + scroll
  - Bidirectional sync: canvas selection ↔ tree highlight
  - ChangeListener lifecycle management (register/unregister)
  - selectedRowIndex preserved across refresh() cycles

affects: [05-04-polish]

tech-stack:
  added: []
  patterns:
    - "refresh() must re-sync selectedRowIndex from canvas selection after rebuild"
    - "Canvas::setSelected → updateCommandStatus → handleAsyncUpdate → refreshObjectsPanel → refresh() = selectedRowIndex always reset without explicit sync"

key-files:
  modified:
    - Source/RepentePd/UI/ObjectTreePanel.h
    - Source/RepentePd/UI/ObjectTreePanel.cpp

key-decisions:
  - "Sync selectedRowIndex at end of refresh() from canvas — not only in changeListenerCallback"
  - "findColour() at paint time instead of PlugDataColours statics (avoids initialization timing issues)"
  - "toolbarActiveColour at 0.35f alpha + text in accent color = visible across all themes"

patterns-established:
  - "Any component that tracks selection state AND participates in refresh() cycles must re-derive state from source of truth at end of refresh()"

duration: ~2h (including debugging)
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 05 Plan 03: Clickable Object Tree Summary

**ObjectTreePanel rows are clickable: click selects canvas object + scrolls into view; canvas selection bidirectionally syncs tree highlight; highlight survives refresh() cycles.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~2h |
| Started | 2026-05-02 |
| Completed | 2026-05-02 |
| Tasks | 2 + 1 checkpoint + 3 post-checkpoint fixes |
| Files modified | 2 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Tree click selects object + scrolls canvas | Pass | mouseDown → deselectAll + setSelected + setViewPosition |
| AC-2: Canvas selection updates tree highlight | Pass | After root-cause fix: refresh() syncs selectedRowIndex from canvas |
| AC-3: Header/empty rows non-interactive | Pass | rowObjects[i] == nullptr guard in mouseDown |

## Accomplishments

- `rowObjects: std::vector<Object*>` parallel to `displayLines` — null for headers/empty, Object* for items
- `mouseDown()`: row hit test → `deselectAll()` + `setSelected()` + `setViewPosition()` to center object
- `changeListenerCallback()`: canvas selection → tree highlight sync via getSelectedItem iteration
- Destructor unregisters ChangeListener from `currentCanvas->selectedComponents`
- Root cause fix: `refresh()` now re-derives `selectedRowIndex` from current canvas selection at rebuild time

## Task Commits

| Task | Commit | Type | Description |
|------|--------|------|-------------|
| Task 1+2: core implementation | `c17e9ac52` | feat | Clickable object tree — select + scroll + bidirectional highlight |
| Post-checkpoint: initial highlight | `55ceb0e66` | feat | Bold + active color + left accent bar for selected row |
| Fix: sidebarActiveBackground | `38112fc44` | fix | Use sidebarActiveBackgroundColour for fill |
| Fix: findColour + higher alpha | `5b7e6c6ee` | fix | findColour at paint time, 0.35f alpha, accent text |
| Fix: root cause | `dfe89bab1` | fix | Preserve tree selection across refresh() calls |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/UI/ObjectTreePanel.h` | Modified | Added ChangeListener inheritance, mouseDown, changeListenerCallback, rowObjects/currentCanvas/selectedRowIndex members |
| `Source/RepentePd/UI/ObjectTreePanel.cpp` | Modified | Full implementation: rowObjects build, mouseDown, changeListenerCallback, paint highlight, destructor, refresh() selection sync |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Sync selectedRowIndex at end of refresh() | `handleAsyncUpdate → refreshObjectsPanel → refresh()` always ran after setSelected, always resetting index | Without this, highlight appeared and was immediately wiped on every click |
| findColour() at paint time | Static PlugDataColours members may not reflect current theme at paint time | Correct colors in all themes |
| toolbarActiveColour at 0.35f alpha | sidebarActiveBackgroundColour (~34 unit contrast on white) too subtle; 0.35f tint clearly visible | Background tint + accent text = two cues |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 1 | Critical — highlight was invisible without root-cause fix |
| Scope additions | 0 | — |
| Deferred | 0 | — |

**Total impact:** One essential post-checkpoint fix; no scope creep.

### Auto-fixed Issues

**1. selectedRowIndex reset by handleAsyncUpdate cycle**
- **Found during:** Checkpoint verification (user reported no visible highlight)
- **Issue:** `Canvas::setSelected()` → `updateCommandStatus()` → `triggerAsyncUpdate()` → `handleAsyncUpdate()` → `refreshObjectsPanel()` → `refresh()` → `selectedRowIndex = -1`. Highlight was drawn then immediately wiped.
- **Fix:** At end of `refresh(Canvas*, Executor*)`, scan `canvas->selectedComponents` and restore `selectedRowIndex` to match currently-selected Object*.
- **Files:** `Source/RepentePd/UI/ObjectTreePanel.cpp`
- **Verification:** User confirmed highlight visible after fix
- **Commit:** `dfe89bab1`

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| Highlight invisible in classic theme (toolbar_active = #787878) | Switched to findColour() + 0.35f alpha + accent text color — multiple visual cues |
| selectedRowIndex reset on every canvas interaction | Root cause: refresh() called by handleAsyncUpdate after every setSelected; fixed by re-syncing at refresh() end |

## Next Phase Readiness

**Ready:**
- ObjectTreePanel fully interactive — foundation for keyboard nav (05-04 deferred)
- Bidirectional sync pattern established

**Concerns:**
- Keyboard arrow navigation still deferred (noted in 05-03 scope limits)
- No multi-selection in tree

**Blockers:**
- None

---
*Phase: 05-polish, Plan: 03*
*Completed: 2026-05-02*
