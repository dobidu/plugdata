# pd-repente — Roadmap

## Milestone 1: MVP (Phases 1–3)
Foundation → pd-script REPL → Repente Bridge. Demo-ready at Phase 3 end.

### Phase 01: Foundation ✓ COMPLETE (2026-04-30)
Fork plugdata, CI matrix (Win/Mac/Linux), PromptBar render-only, baseline JUCE UnitTests.
AC: builds on 3 OSes, PromptBar visible, ctest passes.

### Phase 02: pd-script REPL (2-3w)
CommandParser + sugar pre-processor, DirectCommands (create/connect/delete/list/move/lua),
Executor (pd::Patch wrapper, thread-safe), autocomplete, history, /help, /clear.
AC: /pds create osc~ → object on canvas; audio thread unblocked.

### Phase 03: Repente Bridge (3-4w)
RepenteClient (cpp-httplib, SSE), PdParser (truncation-tolerant), Bridge→Executor,
/config panel, server auto-detect + wizard, model detection warning.
AC: text prompt → valid patch ~5s; Battery B 5/5; server-offline handled.

## Milestone 2: Full UX (Phases 4–5)

### Phase 04: Bidirectionality + Analysis (2-3w)
Canvas serializer (subgraph default/full optional), context injection, Analysis mode,
session persistence (~/.repente-pd/), /canvas, /sessions.
AC: context-aware generation; Battery F completes.

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
