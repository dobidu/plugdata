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
- **macOS CI** — ffmpeg `tls_securetransport.c:168`: `SecIdentityCreate` is 10.12+ but
  `build_ffmpeg.sh:16` pins `-mmacosx-version-min=10.9` under `-Werror`. Passed in May, broke
  2026-07-27 when the runner SDK moved. Patched **CI-side** (2026-07-27) via
  `.github/scripts/patch-ffmpeg-macos-target.sh`, run before configure in `macos-build`
  (pd-repente CI) and `macos-universal-build` (CMake). Not fixed in the submodule — it tracks
  upstream `timothyschoen/pd-else`, which we can't push to; a fork + repoint was considered and
  declined. Consequences: **local macOS builds still fail** unless the script is run by hand, and
  the script hard-fails if upstream restructures the flag, by design. `macos-legacy-build` is
  deliberately unpatched — it passes at 10.9 and its app target is 10.11, so raising ffmpeg to
  10.13 there risks a "built for newer macOS" link warning for no gain.
- Battery B — PdParser routing automated (7 tests); LLM→canvas→audio end-to-end needs manual run
- Battery F — PdParser routing automated (4 tests); LLM→canvas→audio end-to-end needs manual run
- ObjectTreePanel doesn't scan sub-patches (CanvasSerializer now does — Phase 07-01)
- Windows/Linux smoke tests — manual, deferred post-V1
- RepentePd tests run on Linux CI only; macOS/Windows jobs build but don't test
- **`windows-32-build` — FIXED in PR #6 (`f3fb4f96f`), pending merge.** Was pd-repente's own bug. `error C2872: 'ssize_t': ambiguous
  symbol` compiling `Source/RepentePd/Bridge/RepenteClient.cpp`: `long ssize_t` from
  `Libraries/cpp-httplib/httplib.h:143` vs `juce::ssize_t` from
  `Libraries/JUCE/modules/juce_core/maths/juce_MathsFunctions.h:98`. 32-bit MSVC only — the
  64-bit Windows job passes. Arrived with RepenteClient in Phase 03, not with PR #4. Candidate
  fixes: include `httplib.h` ahead of the JUCE headers in that TU, or gate with
  `_SSIZE_T_DEFINED`. Fixed by the former: httplib now precedes the JUCE headers in that TU.
  `windows-32-build` verified green on PR #6 — it had been red since Phase 03.
- **`Arch-x64` / `Arch-aarch64` / `OpenSUSE-Tumbleweed-x64` / `-aarch64` red — plugdata's own,
  not pd-repente. One bug, four jobs, and spreading (OpenSUSE was green until 2026-08-19, which
  fits stricter binutils rather than any source change).** `undefined reference to
  PlugDataWindow::closeAllPatches()`; defined in `Source/Standalone/PlugDataApp.h:233`, declared
  `Source/Standalone/PlugDataWindow.h:514`, called `:510`. A non-inline definition in a header,
  so it links only where that header is compiled — Arch's build reaches the call without the
  definition. Fails identically on `develop`. Fix belongs upstream or in a plugdata-side
  refactor (move the definition into a .cpp or mark it inline); verify against upstream before
  touching it.
- Preset model IDs previous-generation (`claude-opus-4-7`, `claude-sonnet-4-6`); active and error-free, refresh to `claude-opus-5` / `claude-sonnet-5`
- `/listen` lambda captures raw Bridge*/PluginProcessor* — edge case: plugin destroyed while 3.2s timer pending
- Branch topology: `develop` is trunk. `pd-repente-main`, `feat/ci-run-tests`, and
  `fix/ci-linux-glu` were deleted 2026-07-27 after verifying each was fully contained in develop.

## Snapshot / Demo Environment (2026-08-19)

- **WSL2 has no audio** — no ALSA cards, no PulseAudio, JACK absent. plugdata runs
  under WSLg and renders patches, but produces no sound, so `/listen` captures
  silence and any C3/multimodal figure is meaningless there. Capture figures on
  macOS (real audio, built-in `screencapture` + `osascript`, 2x pixel density).
- **v0.7 is not directly servable** — LoRA adapter only (r=32/alpha=64); its
  `adapter_config.json` base is `/workspace/models/base`, a container path. Needs
  base HF weights -> peft merge -> GGUF convert -> quantize -> ollama create.
- **A bare Repente GGUF import answers in SuperCollider** a good share of the time:
  the training system prompt is multi-language ("SuperCollider, Pure Data, and
  MAX/MSP") and pd-repente sends no instructional system prompt of its own. Fixed
  by baking a Pd constraint into the Ollama model — see
  `apps/pd-repente/scripts/Modelfile.repente-pd`.
- Snapshot kit: `apps/pd-repente/docs/SNAPSHOTS.md` +
  `apps/pd-repente/scripts/snapshot-session.sh` (macOS automation).

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
Trunk: `develop` at `aa41ddd17` (PR #4 merged: parser NO_PATCH, CI trigger, macOS ffmpeg).
Open: PR #6 `fix/win32-ssize-t` (`f3fb4f96f`) — windows-32-build green, awaiting merge.
Open: PR #7 `docs/snapshot-kit` — this snapshot kit.
