# pd-repente

## Core Value
plugdata + Repente: compose Pure Data patches by describing what you want to hear.

## Description
plugdata fork integrating Repente LLM as local-first musical composition assistant.
Natural language → valid Pure Data patch on canvas, plays immediately.
Currently ships a full pd-script REPL: create/connect/delete/move objects from a prompt bar, with live object tree sidebar panel.

## Stack
C++17 · JUCE 7.x · libpd · cpp-httplib · nlohmann/json · CMake 3.21+

## Key Constraints
- Cross-platform: Win/Mac/Linux (every dep must build on all three)
- Audio thread NEVER touched by new code
- Repente runs as external HTTP server — no llama.cpp embedding
- OpenAI-compat API only — switching tier = switching URL
- GPL license (plugdata fork)

## Skill Loadout
PAUL · AEGIS (post-Phase 3) · Caveman (Phases 3/5/debug)

## Success Criteria
- Battery B: 5/5 text→canvas→audio tests pass (Phase 3+)
- Battery F: pad→drums→pattern→combined completes (Phase 4)
- CI green Win/Mac/Linux every phase
- V1.0 public release with Ollama support

## Validated Requirements (shipped)

- ✓ CI matrix Win/Mac/Linux — Phase 01
- ✓ PromptBar visible on every canvas — Phase 01
- ✓ pd-script REPL: create/connect/delete/move/list objects — Phase 02
- ✓ Sugar syntax (@type, ~type) — Phase 02
- ✓ Per-canvas registry isolation (tab switch preserves state) — Phase 02
- ✓ ObjectTreePanel: live registry grouped DSP/UI/Control — Phase 02
- ✓ /help and /clear commands — Phase 02
- ✓ Stale registry cleanup on tab close (partial) — Phase 02
- ✓ Lua+pds API: pds.create/connect/delete/move/list callable in Lua — Phase 02
- ✓ /help <topic>: paged help (pds/sugar/lua/llm/commands/builtin) — Phase 02
- ✓ ObjectTreePanel syncs on GUI-driven object deletion — Phase 02
- ✓ RepenteClient: async HTTP POST + ping, cancel-token safe shutdown — Phase 03
- ✓ PdParser: format detection (PD_PATCH / LUA_BLOCK / PDS_COMMANDS) — Phase 03
- ✓ Bridge: RepenteClient → PdParser → canvas execution — Phase 03
- ✓ /config command: url/model/key/test; persists via SettingsFile — Phase 03
- ✓ CanvasSerializer: canvas → pd-file format for LLM context injection — Phase 04
- ✓ ObjectTreePanel extended: full canvas scan (GUI-added objects) — Phase 04
- ✓ /analyze mode: LLM query without patch execution — Phase 04
- ✓ Multi-turn conversation history (20-turn rolling window) — Phase 04
- ✓ History persistence via SettingsFile (opt-in) — Phase 04
- ✓ Merge mode: LLM patch merged into current canvas — Phase 04
- ✓ Console type-3 teal for repente status messages — Phase 04

## Validated Requirements (shipped — Phase 06)

- ✓ AudioCapture: wait-free thread-safe tap from processBlock; startCapture/takeCapture API — Phase 06-01
- ✓ SpectralAnalyzer: multi-frame FFT → 5 frequency bands + top-3 peak frequencies → LLM text — Phase 06-02
- ✓ Bridge.send() audioContext param: canvas + spectral injected as single system message — Phase 06-03
- ✓ /listen [prompt] command: capture 3s audio → analyze → inject → LLM refine — Phase 06-03

## Active Requirements

- [ ] ObjectTreePanel: full canvas scan (sub-patches) — deferred post-V1
- [~] Battery B: 5/5 text→canvas→audio — PdParser routing + fence stripping automated (7 tests); LLM→canvas→audio end-to-end pending manual run
- [~] Battery F: pad→drums→pattern→combined — PdParser routing automated (4 tests); LLM→canvas→audio end-to-end pending manual run
- [ ] Windows/Linux smoke tests — post-V1

## Key Decisions

| Decision | Phase | Rationale |
|----------|-------|-----------|
| No OutputArea component — use existing console tab | 02-03 | Avoids UX fragmentation; pd->logMessage already routes there |
| Separate `PluginEditor*` in PromptInput | 02-03 | CommandInput::editor is private |
| ObjectEntry stores ptr+text — classification in display layer | 02-03 | Executor stays generic; ObjectTreePanel owns grouping logic |
| Tab-switch side-effects in handleAsyncUpdate | 02-03 | Single hook for all tab switches alongside setCanvas |
| SugarExpander as pre-processor, not parser | 02-02 | Clean separation: sugar→canonical text→parse→execute |
| Executor per-canvas CanvasState map | 02-02 | Isolates registry per tab; save/restore on switch |
| pds Lua closures as private static members of PromptInput | 02-04 | File-scope statics can't access class-private fields |
| pruneDeletedObjects hooked into Canvas::performSynchronise | 02-04 | Cheapest hook — covers GUI delete, undo, all pd mutations |
| LLM pd-patch responses open as new tab (not merged) | 03 | Non-destructive; plugdata handles .pd load natively via temp file |
| LLM response format auto-detected: #N canvas → patch; pds./Lua → Lua engine; /pds lines → Executor | 03 | Single parser handles all response types; no LLM prompt constraints |
| Cancel token via `shared_ptr<atomic<bool>>` | 03-03 | Detached threads outlive RepenteClient on quit; destructor must safely signal |
| SettingsFile custom keys must be in defaultSettings map | 03-03 | saveSettings() uses .at(name) — throws out_of_range for unknown keys |
| Canvas serialized to pd-file format (not JSON) | 04-01 | Native LLM format; pd-file is self-documenting for the model |
| ObjectTreePanel uses Canvas as source of truth | 04-01 | Executor overlays REPL names; full scan shows GUI-added objects too |
| Bridge owns message construction; RepenteClient is transport-only | 04-03 | Clean separation; no history logic leaks into HTTP layer |
| History persistence off by default; opt-in via /config history on | 04-03 | Avoids silent accumulation; user controls privacy |
| JUCE JSON (DynamicObject) for history serialization | 04-03 | No new dep; nlohmann/json include chain not clean in Bridge.cpp |
| refresh() must re-sync selectedRowIndex from canvas after rebuild | 05-03 | handleAsyncUpdate resets it; re-derive from source of truth |
| Console-only first-launch wizard (no modal) | 05-04 | Consistent with pd-repente UX; logRepente is the primary channel |
| Sequential auto-detect pings (Ollama → repente server) | 05-04 | Avoids race condition; simple state machine |
| AudioCapture: push from audio thread (wait-free atomics), pull from message thread | 06-01 | No mutex in feedAudio; audio thread never blocks |
| SpectralAnalyzer: stateless static methods, juce::dsp::FFT constructed per call | 06-02 | No state between /listen calls; FFT overhead acceptable for non-real-time use |
| audioContext appended to canvas context in single system message (newline-separated) | 06-03 | One system message; LLM gets canvas + spectral together |
| juce::Timer::callAfterDelay for async capture wait (not MessageManager) | 06-03 | Correct JUCE API; MessageManager has no callAfterDelay |

## Validated Requirements (shipped — Phase 05)

- ✓ CanvasLayouter auto-placement via ObjectGrid::positionNewObject() — Phase 05-01
- ✓ /arrange: signal-flow topology BFS layout — Phase 05-02
- ✓ ObjectTreePanel: clickable rows, canvas↔tree bidirectional selection sync — Phase 05-03
- ✓ Ollama auto-detect on first launch (localhost:11434 → localhost:7860) — Phase 05-04
- ✓ Privacy warning on non-localhost URL configuration — Phase 05-04
- ✓ /help llm: Ollama + OpenAI + repente server setup examples — Phase 05-04

---
*Last updated: 2026-05-05 — Phase 06 complete · Milestone 3 complete*
