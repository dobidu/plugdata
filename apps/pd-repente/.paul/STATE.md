# PAUL State

## Loop Position
```
PLAN ──▶ APPLY ──▶ UNIFY
  —        ✓        ✓     [Phase 07 reconciled retroactively — Milestone 4 complete]
```

## Active Milestone
Milestone 4 — Multi-Provider + CI Hardening · ✅ COMPLETE

## Active Phase
Phase 07: providers-ci ✅ COMPLETE (3/3 plans, retro-documented)

## Active Plan
— (Phase 07 complete · next milestone TBD)

## Last Action
2026-07-26 — UNIFY reconciled 13 off-roadmap commits into Phase 07; STATE git pointers corrected

## Next Action
Choose next milestone (backlog: Style Transfer Sonoro, live coding latency, Tier 3)

## Progress
- Phase 01: 100% ✅
- Phase 02: 100% ✅
- Phase 03: 100% ✅
- Phase 04: 100% ✅
- Phase 05: 100% ✅ (4/4 plans)
- Phase 06: 100% ✅ (3/3 plans)
- Phase 07: 100% ✅ (3/3 plans, reconstructed)

**Milestone 2: 100% ✅ — V1.0 complete**
**Milestone 3: 100% ✅ — Multimodal Loop complete**
**Milestone 4: 100% ✅ — Multi-provider + CI hardening complete**

## Session Continuity
Last session: 2026-07-26
Stopped at: Phase 07 UNIFY complete (retroactive reconcile)
Next action: Choose next milestone from backlog
Resume file: .paul/ROADMAP.md

## Open Items
- ffmpeg `build_ffmpeg.sh` 10.9→10.13 patch: must commit to pd-else submodule for macOS CI
- Battery B — PdParser routing automated (7 tests); LLM→canvas→audio end-to-end needs manual run
- Battery F — PdParser routing automated (4 tests); LLM→canvas→audio end-to-end needs manual run
- ObjectTreePanel doesn't scan sub-patches (CanvasSerializer now does — Phase 07-01)
- Windows/Linux smoke tests — manual, deferred post-V1
- RepentePd tests run on Linux CI only; macOS/Windows jobs build but don't test
- `CMake` full matrix: `Arch-x64`, `Arch-aarch64`, `windows-32-build` fail — pre-existing, predates Phase 07, unrelated to pd-repente
- Preset model IDs previous-generation (`claude-opus-4-7`, `claude-sonnet-4-6`); active and error-free, refresh to `claude-opus-5` / `claude-sonnet-5`
- `/listen` lambda captures raw Bridge*/PluginProcessor* — edge case: plugin destroyed while 3.2s timer pending
- Branch topology: `develop` is trunk; `pd-repente-main` is stale at `dbb1255b9` and should be deleted or fast-forwarded

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
- AudioCapture: push from audio thread (wait-free atomic), pull from message thread — no mutex in feedAudio (Phase 06-01)
- SpectralAnalyzer: stateless static methods; juce::dsp::FFT constructed per analyze() call — acceptable for non-real-time /listen (Phase 06-02)
- audioContext appended to canvas context (newline-separated) in single system message — not a separate message (Phase 06-03)
- juce::Timer::callAfterDelay for async capture wait — MessageManager has no callAfterDelay (Phase 06-03)
- CanvasSerializer delegates to patch.getCanvasContent() — libpd binbuf recurses for free (Phase 07-01)
- Provider owns request/response shape; RepenteClient is transport-only (Phase 07-02)
- Presets as user-overridable JSON, not hardcoded enums (Phase 07-02)
- CI test gate lives in PlugDataApp::initialise, before StandalonePluginHolder (Phase 07-03)
- CMake must emit ENABLE_TESTING=1/0 — bare `ON` is 0 to the preprocessor (Phase 07-03)

### Git State
Last commit: 2bbf96810 (`feat/ci-run-tests`), merged to `origin/develop` as 977cd96e5 (PR #3)
Trunk: `develop`
