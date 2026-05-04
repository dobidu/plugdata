# PAUL State

## Loop Position
```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Milestone 2 COMPLETE — V1.0]
```

## Active Milestone
— (Milestone 2 complete · post-V1 backlog next)

## Active Phase
— (Phase 05 complete)

## Active Plan
—

## Last Action
2026-05-04 — Phase 05 UNIFY complete · Milestone 2 closed · V1.0

## Next Action
Post-V1 backlog planning (see ROADMAP.md Backlog section)

## Progress
- Phase 01: 100% ✅
- Phase 02: 100% ✅
- Phase 03: 100% ✅
- Phase 04: 100% ✅
- Phase 05: 100% ✅ (4/4 plans)

**Milestone 2: 100% ✅ — V1.0 complete**

## Session Continuity
Last session: 2026-05-04
Stopped at: Phase 05 complete — V1.0 milestone closed
Next action: Post-V1 backlog (see ROADMAP.md Backlog section)
Resume file: .paul/ROADMAP.md

## Open Items
- ffmpeg `build_ffmpeg.sh` 10.9→10.13 patch: must commit to pd-else submodule for macOS CI
- Battery B (5/5 text→canvas→audio tests) — not yet verified end-to-end
- Battery F (pad→drums→pattern→combined) — not yet verified end-to-end
- CanvasSerializer doesn't recurse into sub-patches — top-level only
- Windows/Linux smoke tests — manual, deferred post-V1

## Accumulated Context

### Decisions
- Canvas serialized to pd-file format (not JSON) — native LLM format (Phase 04)
- systemContext omitted from request when canvas empty (Phase 04)
- ObjectTreePanel uses Canvas as source of truth; Executor overlays REPL names (Phase 04)
- Bridge owns message construction; RepenteClient is transport-only (Phase 04)
- History persistence off by default; opt-in via /config history on (Phase 04)
- NEVER use patch.moveObjectTo for programmatic repositioning — adds +1542 offset; use getObjectBounds + patch.moveObjects delta instead (Phase 05-02)
- Cancel token (`shared_ptr<atomic<bool>>`) for all detached threads in RepentePd (Phase 03)
- SettingsFile custom keys must be registered in defaultSettings map (Phase 03)
- CONFIGURE_DEPENDS on cmake source glob — auto-picks new Bridge/*.cpp (Phase 03)
- refresh() must re-sync selectedRowIndex from canvas after rebuild — handleAsyncUpdate resets it (Phase 05-03)
- findColour() at paint time, not PlugDataColours statics, for reliable theme colors (Phase 05-03)
- Console-only first-launch wizard; no modal — consistent with pd-repente UX (Phase 05-04)
- Sequential auto-detect pings (Ollama → repente server) — avoids race condition (Phase 05-04)

### Git State
Last commit: f60dd53fc
Branch: pd-repente-main
