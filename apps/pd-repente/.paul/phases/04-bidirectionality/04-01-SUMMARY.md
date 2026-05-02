---
phase: 04-bidirectionality
plan: 01
subsystem: bridge
tags: [canvas-serializer, context-injection, objecttreepanel, llm, system-message]

requires:
  - phase: 03-03
    provides: Bridge + RepenteClient + /config + SettingsFile persistence

provides:
  - CanvasSerializer: serialize canvas → pd-file format
  - Context injection: every LLM request includes canvas state as role:system
  - /canvas debug command
  - ObjectTreePanel full canvas scan (pulled from 04-02)
  - refreshObjectsPanel reads Canvas directly, not just Executor registry

affects: [04-02, 04-03, 05-polish]

tech-stack:
  added: []
  patterns:
    - "Canvas serialization: iterate canvas->objects + canvas->connections, emit pd-file format"
    - "systemContext param on RepenteClient::send() — omitted from request when empty (AC-3)"
    - "ObjectTreePanel::refresh(Canvas*, Executor*) — ptr-based REPL name lookup via indexMap"

key-files:
  created:
    - Source/RepentePd/Bridge/CanvasSerializer.h
    - Source/RepentePd/Bridge/CanvasSerializer.cpp
  modified:
    - Source/RepentePd/Bridge/RepenteClient.h
    - Source/RepentePd/Bridge/RepenteClient.cpp
    - Source/RepentePd/Bridge/Bridge.cpp
    - Source/RepentePd/UI/PromptInput.cpp
    - Source/RepentePd/UI/ObjectTreePanel.h
    - Source/RepentePd/UI/ObjectTreePanel.cpp
    - Source/PluginEditor.cpp
    - apps/pd-repente/README.md

key-decisions:
  - "pd-file format for canvas context (not JSON) — LLMs trained on it; no translation"
  - "systemContext omitted when canvas empty — avoids empty system message on blank patch"
  - "ObjectTreePanel pulled from 04-02 into 04-01 — needed for checkpoint to pass"
  - "REPL name lookup via ptr map (ObjectEntry::ptr → name) — reliable identity across renames"

patterns-established:
  - "ObjectTreePanel::refresh(Canvas*, Executor*): Canvas = source of truth; Executor provides names"
  - "Unnamed canvas objects shown as [type]; REPL-named shown as name  [type]"

duration: ~3h
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 04 Plan 01: Canvas Serializer + Context Injection Summary

**CanvasSerializer serializes active canvas to pd-file format; every LLM request now includes canvas state as a system message; ObjectTreePanel shows all canvas objects (pulled from 04-02 scope).**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~3h |
| Started | 2026-05-02 |
| Completed | 2026-05-02 |
| Tasks | 2 auto + 1 checkpoint + 2 scope additions |
| Files modified | 10 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Canvas state injected into LLM prompt | Pass | system message with pd-format snapshot in every request |
| AC-2: /canvas shows serialized state | Pass | shows #N canvas header + #X obj lines + #X connect lines |
| AC-3: Empty canvas handled gracefully | Pass | system message omitted entirely when canvas.objects.empty() |

## Accomplishments

- `CanvasSerializer::serialize()` iterates `canvas->objects` and `canvas->connections`, emits valid pd-file format; REPL names overlaid from Executor ptr map
- `RepenteClient::send()` gains `systemContext` param — injected as `{"role": "system"}` when non-empty
- `Bridge::send()` calls serializer before every HTTP request; empty canvas → no system message
- `/canvas` debug command shows LLM context in console
- `ObjectTreePanel::refresh(Canvas*, Executor*)` added — full canvas scan replacing registry-only view; REPL objects show with name, others as `[type]`
- `refreshObjectsPanel()` now passes `getCurrentCanvas()` to panel

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Tasks 1+2: serializer + injection | `ecac0b80c` | CanvasSerializer + RepenteClient systemContext + Bridge + /canvas |
| Fix: panel refresh after openPatch | `aca3c75f2` | Bridge::execute() calls refreshObjectsPanel() after patch opens |
| Scope addition: full canvas scan | `fd83981ad` | ObjectTreePanel canvas-based refresh + README update |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Bridge/CanvasSerializer.h` | Created | Static serialize(Canvas*) → pd-file string |
| `Source/RepentePd/Bridge/CanvasSerializer.cpp` | Created | Iterate objects + connections, build pd snippet |
| `Source/RepentePd/Bridge/RepenteClient.h` | Modified | send() gains systemContext parameter |
| `Source/RepentePd/Bridge/RepenteClient.cpp` | Modified | system message prepended when systemContext non-empty |
| `Source/RepentePd/Bridge/Bridge.cpp` | Modified | CanvasSerializer call + refreshObjectsPanel() after openPatch |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | /canvas command + /help commands updated |
| `Source/RepentePd/UI/ObjectTreePanel.h` | Modified | Added refresh(Canvas*, Executor*) overload |
| `Source/RepentePd/UI/ObjectTreePanel.cpp` | Modified | Canvas scan impl with ptr→name lookup |
| `Source/PluginEditor.cpp` | Modified | refreshObjectsPanel() passes canvas + executor |
| `apps/pd-repente/README.md` | Modified | Full rewrite reflecting Phases 01-03 complete + Phase 04 state |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| pd-file format for canvas context | LLMs trained on pd syntax; no translation layer needed | Simpler than JSON; model understands object graph natively |
| Omit system message on empty canvas | Empty string system message is noise; some models reject it | AC-3 pass; clean request on blank patches |
| ObjectTreePanel pulled into 04-01 | LLM-opened tab showed no objects — checkpoint couldn't pass without it | 04-02 scope needs new focus (now free for analysis mode or sessions) |
| ptr-based REPL name lookup | ObjectEntry stores void* ptr — reliable even if text changes | Correct labeling of REPL vs canvas objects in panel |

## Deviations from Plan

| Type | Count | Impact |
|------|-------|--------|
| Scope additions | 2 | Essential for checkpoint — no scope creep |
| Auto-fixed | 0 | — |
| Deferred | 0 | — |

### Scope addition 1: refreshObjectsPanel() after openPatch()
- **Found during:** Checkpoint verification (LLM-opened tab, panel showed nothing)
- **Issue:** Bridge::execute() opened patch but never triggered panel refresh
- **Fix:** Added `editor->refreshObjectsPanel()` after `openPatch()` in Bridge::execute()
- **Commit:** `aca3c75f2`

### Scope addition 2: ObjectTreePanel full canvas scan
- **Found during:** Checkpoint — even after refresh, panel still empty because it only read Executor registry
- **Root cause:** LLM-generated objects are not in Executor registry; panel needed Canvas as source of truth
- **Fix:** Added `refresh(Canvas*, Executor*)` overload; `refreshObjectsPanel()` passes canvas
- **Note:** This was planned for 04-02; pulled forward because it was a hard blocker for 04-01 checkpoint
- **Commit:** `fd83981ad`

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| ObjectTreePanel showed nothing for LLM-generated tab | Full canvas scan pulled from 04-02 into this plan |

## Next Phase Readiness

**Ready:**
- Context-aware generation live: LLM receives full canvas state on every request
- ObjectTreePanel shows all canvas objects (REPL-named + GUI-placed + LLM-generated)
- `/canvas` available for debugging what the LLM sees

**Concerns:**
- 04-02 was "ObjectTreePanel full scan" — that's done. 04-02 scope needs replanning.
  Candidates: Analysis mode (read-only LLM analysis without execution) or session persistence.
- CanvasSerializer does not recurse into sub-patches — only active top-level canvas

**Blockers:** None

---
*Phase: 04-bidirectionality, Plan: 01*
*Completed: 2026-05-02*
