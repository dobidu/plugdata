# pd-repente — Roadmap

## Milestone 1: MVP (Phases 1–3)
Foundation → pd-script REPL → Repente Bridge. Demo-ready at Phase 3 end.

### Phase 01: Foundation ✓ COMPLETE (2026-04-30)
Fork plugdata, CI matrix (Win/Mac/Linux), PromptBar render-only, baseline JUCE UnitTests.
AC: builds on 3 OSes, PromptBar visible, ctest passes.

### Phase 02: pd-script REPL ✅ COMPLETE (2026-05-01)
Plans: 01 ✓ | 02 ✓ | 03 ✓ | 04 ✓ (4/4)

CommandParser + sugar pre-processor, DirectCommands (create/connect/delete/list/move),
Executor (pd::Patch wrapper, thread-safe, per-canvas registry), history, /help <topic>, /clear.
ObjectTreePanel (compact sidebar panel, objects grouped DSP/control/UI, REPL-named objects only).
Lua+pds API: pds.create/connect/delete/move/list callable from Lua blocks — synchronous,
message-thread-safe; enables loops and generative patch scripting.
/help <topic>: pds | sugar | lua | pds-lua | llm | commands.
AC: /pds create osc~ → object on canvas; audio thread unblocked;
    console shows result; object tree reflects live registry;
    Lua loop creates N objects via pds.create().

### Phase 03: Repente Bridge ✅ COMPLETE (2026-05-02)
Plans: 01 ✓ | 02 ✓ | 03 ✓ (3/3)

RepenteClient (cpp-httplib, async POST + ping), PdParser (PD_PATCH/LUA_BLOCK/PDS_COMMANDS),
Bridge→Executor full wiring, /config command (url/model/key/test), SettingsFile persistence,
cancel-token for safe shutdown.
AC: free text → LLM → canvas live (Ollama tested); /config persists; quit crash-free.

## Milestone 2: Full UX (Phases 4–5) ✅ COMPLETE (2026-05-04)

### Phase 04: Bidirectionality + Analysis ✅ COMPLETE (2026-05-02)
Plans: 01 ✓ | 02 ✓ | 03 ✓ (3/3)

CanvasSerializer (pd-file format, top-level objects), context injection per-request,
/analyze mode (LLM query without execution), merge mode (LLM patch merged into canvas),
console type-3 teal for repente status, multi-turn conversation history (20-turn rolling),
history persistence opt-in (/config history on|off), /history + /history clear commands,
/config expanded (history, merge), startup focus on prompt bar.
AC: context-aware generation; multi-turn iterative builds; /analyze text-only; history survives restart.

### Phase 05: Tier 2 + V1.0 Polish ✅ COMPLETE (2026-05-04)
Plans: 01 ✓ | 02 ✓ | 03 ✓ | 04 ✓ (4/4)

**05-01: CanvasLayouter — auto-placement (2 plans)**
Existing plugdata infrastructure to reuse:
  - ObjectGrid::positionNewObject() — snaps new objects (edge/center/grid); used by GUI creation
  - Canvas::alignObjects(Align) — 8 alignment types (L/R/center/top/bottom/HDistribute/VDistribute)
  - Canvas::tidySelection() — delegates to pd native tidy; reorganizes selection
  - duplicateSelection() — overlap-aware offset; avoids stacking on paste
  - SnapSettings — configurable grid size, edge/center snap modes

Plan 1 — Auto-placement: route Executor create + PatchMerger through ObjectGrid::positionNewObject()
instead of using raw LLM coordinates or fixed offsets. `/pds create osc~` without x/y picks next
available slot. Merge mode discards LLM coordinates entirely; layout handled by plugdata's snap system.
Plan 2 — Signal-flow arrange: topology-aware DAG (sources top → processors → sinks).
`/arrange` command calls alignObjects + custom sort by connection depth. Type classification table
(osc~/noise~ = source, dac~/throw~ = sink, filter~/reverb~ = processor). Builds on Plan 1.

**05-02: Clickable Object Tree**
ObjectTreePanel becomes interactive: clicking an object row selects it on the canvas
(Canvas::setSelected / selectObjectWithEdges or equivalent). Keyboard arrow navigation
through rows optional. Selection syncs both ways: canvas click → tree highlight,
tree click → canvas select + scroll into view.
AC: clicking object in tree selects it on canvas; canvas scrolls to show it.

**05-03: V1.0 Polish**
Ollama auto-detect (localhost:11434), first-launch wizard, privacy warning,
user docs, smoke tests 3 OSes. V1.0 public release.
AC: Ollama detects without manual config; privacy warning on remote URL.

## Milestone 3: Multimodal Loop — C3 Tríade ✅ COMPLETE (2026-05-05)

### Phase 06: Multimodal Loop ✅ COMPLETE (2026-05-05)
Plans: 01 ✓ | 02 ✓ | 03 ✓ (3/3)

**06-01: AudioCapture** — Wait-free processBlock tap; startCapture(durationSec)/feedAudio/takeCapture() API.
**06-02: SpectralAnalyzer** — Multi-frame FFT (2048pt Hann, 50% overlap); 5 frequency bands + top-3 peak frequencies → LLM text.
**06-03: Bridge + /listen** — Bridge.send() audioContext param; `/listen [prompt]` captures 3s → analyzes → injects canvas + spectral as system context.

AC: `/listen` captures 3s audio, analyzes spectrum, sends canvas + spectral as LLM context. ✓

## Milestone 4: Multi-Provider + CI Hardening ✅ COMPLETE (2026-05-20)

### Phase 07: Provider Abstraction & CI Automation ✅ COMPLETE (2026-05-20)
Plans: 01 ✓ | 02 ✓ | 03 ✓ (3/3) — *retro-documented 2026-07-26; shipped off-roadmap, no PLAN.md*

**07-01: Post-M3 hardening** — `/listen` analyzeOnly when promptless; ` > ` routing narrowed to
single-token lhs; CanvasSerializer delegates to libpd binbuf (sub-patches/graphs/arrays now
included); `/arrange [direction] [step]` configurable.
**07-02: LLM provider abstraction** — ILlmProvider + OpenAIProvider + AnthropicProvider
(native Messages API, prompt-cached system block); PresetLoader with user-override JSON;
`/config provider|preset`; per-provider key storage. RepenteClient reduced to transport.
**07-03: CI unit-test automation** — `PLUGDATA_CI_TESTS_ONLY=1` gate in `PlugDataApp::initialise`;
`ENABLE_TESTING=1` numeric fix; xvfb + timeout; RepentePd category runs on every push (Linux).

AC: provider swap without transport changes ✓ · `/config preset claude-opus` switches backend ✓ ·
tests run and gate every push, `pd-repente CI` green on all three OSes ✓

## Backlog (post-V1)
- Fix `ssize_t` ambiguity in RepenteClient.cpp on 32-bit MSVC (cpp-httplib vs juce) — `windows-32-build` red
- Fix `PlugDataWindow::closeAllPatches()` link error on Arch (header-defined non-inline fn) — upstream-side
- Refresh preset model IDs to current generation (`claude-opus-5`, `claude-sonnet-5`)
- Run RepentePd tests on macOS + Windows CI (Linux-only today)
- Lexical Pd knowledge base injected as retrieval context (canonical object chains for FM/AM/filters/envelopes/…), keyword-matched, no embeddings — deferred so `PromptNormalizer` can be evaluated as the single changed variable first
- Live coding latency (P2 persona)
- Tier 3: PatchBox / Orange Pi (3B distilled)
- SC/MAX cross-language (when Repente reincorporates)
- Style Transfer Sonoro (depends on Phase 06)
