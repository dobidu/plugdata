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

## Active Requirements

- [ ] RepenteClient: async HTTP POST to OpenAI-compat endpoint, callback on message thread — Phase 03
- [ ] LLM response auto-detection: pd patch text → new tab; Lua/pds code → Lua engine; /pds lines → Executor — Phase 03
- [ ] PdParser: detect response format, write temp .pd file for patch responses, open as new tab — Phase 03
- [ ] Bridge: connects RepenteClient → PdParser → execution path; wires PromptInput free-text — Phase 03
- [ ] /config panel: server URL, model, API key, server auto-detect — Phase 03
- [ ] Canvas serializer for context injection — Phase 04
- [ ] ObjectTreePanel: full canvas scan (GUI-added + sub-objects) — Phase 04

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

---
*Last updated: 2026-05-01 — Phase 03 requirements confirmed*
