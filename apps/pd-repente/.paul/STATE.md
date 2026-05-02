# PAUL State

## Loop Position
```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ○        ○     [Plan created — awaiting approval]
```

## Active Milestone
Milestone 2: Full UX

## Active Phase
04-bidirectionality (Plan 2 of 3 complete)

## Active Plan
04-03 — Session persistence (conversation history + /history command)

## Last Action
2026-05-02 — 04-03 PLAN created

## Next Action
/paul:apply 04-bidirectionality/04-03

## Progress
- Phase 01: 100% ✅
- Phase 02: 100% ✅
- Phase 03: 100% ✅
- Phase 04: 66% — 2/3 plans complete (04-03 planned)

## Session Continuity
Last session: 2026-05-02
Stopped at: 04-03 PLAN complete, awaiting APPLY
Next action: /paul:apply 04-bidirectionality/04-03
Resume file: .paul/phases/04-bidirectionality/04-03-PLAN.md

## Open Items
- ffmpeg `build_ffmpeg.sh` 10.9→10.13 patch: must commit to pd-else submodule for macOS CI
- canvasStates stale entries on context-menu tab close — benign, will GC on canvas destroy
- Battery B (5/5 text→canvas→audio tests) — Phase 04+ goal
- CanvasSerializer doesn't recurse into sub-patches — top-level only for now
- Phase 05-01: CanvasLayouter — auto-placement (plan 1) + signal-flow layout (plan 2); see ROADMAP
- Phase 05-02: Clickable ObjectTreePanel — tree row click selects object on canvas + scroll into view

## Accumulated Context

### Decisions
- Canvas serialized to pd-file format (not JSON) — native LLM format (Phase 04)
- systemContext omitted from request when canvas empty (Phase 04)
- ObjectTreePanel uses Canvas as source of truth; Executor overlays REPL names (Phase 04)
- Cancel token (`shared_ptr<atomic<bool>>`) for all detached threads in RepentePd (Phase 03)
- SettingsFile custom keys must be registered in defaultSettings map (Phase 03)
- CONFIGURE_DEPENDS on cmake source glob — auto-picks new Bridge/*.cpp (Phase 03)

### Git State
Last commit: fd83981ad (04-01 UNIFY)
Branch: pd-repente-main
