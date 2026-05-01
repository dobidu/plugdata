# pd-repente — Roadmap

## Milestone 1: MVP (Phases 1–3)
Foundation → pd-script REPL → Repente Bridge. Demo-ready at Phase 3 end.

### Phase 01: Foundation ✓ COMPLETE (2026-04-30)
Fork plugdata, CI matrix (Win/Mac/Linux), PromptBar render-only, baseline JUCE UnitTests.
AC: builds on 3 OSes, PromptBar visible, ctest passes.

### Phase 02: pd-script REPL (in progress — 3/4 plans complete)
Plans: 01 ✓ REPL engine | 02 ✓ DirectCommands + SugarExpander + PromptInput | 03 ✓ ObjectTreePanel + /help + /clear | 04 Lua+pds bindings + /help topics

CommandParser + sugar pre-processor, DirectCommands (create/connect/delete/list/move),
Executor (pd::Patch wrapper, thread-safe, per-canvas registry), history, /help <topic>, /clear.
ObjectTreePanel (compact sidebar panel, objects grouped DSP/control/UI, REPL-named objects only).
Lua+pds API: pds.create/connect/delete/move/list callable from Lua blocks — synchronous,
message-thread-safe; enables loops and generative patch scripting.
/help <topic>: pds | sugar | lua | pds-lua | llm | commands.
AC: /pds create osc~ → object on canvas; audio thread unblocked;
    console shows result; object tree reflects live registry;
    Lua loop creates N objects via pds.create().

### Phase 03: Repente Bridge (3-4w)
RepenteClient (cpp-httplib, SSE), PdParser (truncation-tolerant), Bridge→Executor,
/config panel, server auto-detect + wizard, model detection warning.
AC: text prompt → valid patch ~5s; Battery B 5/5; server-offline handled.

## Milestone 2: Full UX (Phases 4–5)

### Phase 04: Bidirectionality + Analysis (2-3w)
Canvas serializer (subgraph default/full optional), context injection, Analysis mode,
session persistence (~/.repente-pd/), /canvas, /sessions.
ObjectTreePanel extended: scan ALL canvas objects (REPL-named + GUI-added + sub-patch objects);
unnamed objects shown read-only as [type]; composed objects/abstractions expandable one level.
AC: context-aware generation; Battery F completes; object tree shows full patch state.

### Phase 05: Tier 2 + V1.0 Polish (2w)
Ollama auto-detect (localhost:11434), first-launch wizard, privacy warning,
user docs, smoke tests 3 OSes. V1.0 public release.
AC: Ollama detects without manual config; privacy warning on remote URL.

## Backlog (post-V1)
- Live coding latency (P2 persona)
- Tier 3: PatchBox / Orange Pi (3B distilled)
- Multimodal loop — audio render → spectral analysis (C3 Tríade)
- SC/MAX cross-language (when Repente reincorporates)
- Style Transfer Sonoro
