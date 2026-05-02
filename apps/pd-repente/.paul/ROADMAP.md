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

## Milestone 2: Full UX (Phases 4–5)

### Phase 04: Bidirectionality + Analysis (2-3w)
Canvas serializer (subgraph default/full optional), context injection, Analysis mode,
session persistence (~/.repente-pd/), /canvas, /sessions.
ObjectTreePanel extended: scan ALL canvas objects (REPL-named + GUI-added + sub-patch objects);
unnamed objects shown read-only as [type]; composed objects/abstractions expandable one level.
AC: context-aware generation; Battery F completes; object tree shows full patch state.

### Phase 05: Tier 2 + V1.0 Polish (3-4w)
Plans: 01 · 02 · 03 · 04

**05-01: CanvasLayouter — auto-placement (2 plans)**
Plan 1 — Grid auto-placement: scan existing object bounding boxes, pack new objects into next
available ~80px grid cell. Merge mode ignores LLM coordinates; uses CanvasLayouter instead.
`/pds create osc~` without x/y auto-places. No coordinate arithmetic from user or LLM.
Plan 2 — Signal-flow layout: topology-aware DAG arrangement (sources top → processors → sinks).
`/arrange` command reorganizes active canvas. Type classification table (osc~/noise~ = source,
dac~/throw~ = sink, filter~/reverb~ = processor).

**05-02: V1.0 Polish**
Ollama auto-detect (localhost:11434), first-launch wizard, privacy warning,
user docs, smoke tests 3 OSes. V1.0 public release.
AC: Ollama detects without manual config; privacy warning on remote URL.

## Backlog (post-V1)
- Live coding latency (P2 persona)
- Tier 3: PatchBox / Orange Pi (3B distilled)
- Multimodal loop — audio render → spectral analysis (C3 Tríade)
- SC/MAX cross-language (when Repente reincorporates)
- Style Transfer Sonoro
