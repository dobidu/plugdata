# pd-repente

> plugdata + Repente: compose Pure Data patches by describing what you want to hear.

**Type:** Application
**Stack:** C++17 · JUCE 7.x · libpd · cpp-httplib · nlohmann/json · CMake 3.21+
**Skill Loadout:** PAUL (required), AEGIS (post-Phase 3), Caveman (Phases 3/5/debug)
**Quality Gates:** JUCE UnitTests · Battery B/F benchmarks · CI matrix (Win/Mac/Linux)
**License:** GPL (plugdata fork)

---

## Overview

`pd-repente` is a fork of [plugdata](https://github.com/plugdata-team/plugdata) that integrates the [Repente](https://github.com/ggerganov/llama.cpp) LLM as a local-first musical composition assistant. Type a description of what you want to hear; a valid Pure Data patch appears on the canvas and plays immediately.

No chat interface, no copy-paste workflow. The patch is the output.

**Primary target:** sound designers and composers (P1 — Tier 1 hardware: RTX 5070 / M4+). Live coding latency optimization is V2.

**Not in scope (V1):** Max/MSP, SuperCollider, embedded targets, multi-user, cloud inference.

---

## Architecture

```
User input (free text or /pds command)
         │
    PROMPT BAR (JUCE Component)
    ┌────────────────────────────┐
    │  Output Area (scrollable,  │
    │  collapsible, above input) │
    │  ─────────────────────     │
    │  > input field + status    │
    └────────────────────────────┘
         │
    ┌────┴────────────────┐
    │                     │
PD-SCRIPT ENGINE     REPENTE BRIDGE
(CommandParser +     (RepenteClient,
 sugar pre-proc)      SSE streaming,
    │                  PdParser)
    └────────┬──────────┘
             │
        EXECUTOR
        (pd::Patch wrapper,
         message thread only)
             │
       PURE DATA CANVAS (libpd)
       [updates immediately on pd-script]
       [updates atomically on Repente [DONE]]
```

**Threading model (critical):**
- Audio thread: never touched by new code
- HTTP thread (`juce::Thread`): all Repente calls
- Canvas mutations: message thread only via `MessageManager::callAsync`
- Enforced with `JUCE_ASSERT_MESSAGE_THREAD` in Executor

---

## Stack

| Layer | Choice | Rationale |
|---|---|---|
| Host | C++17 + JUCE 7.x + libpd | plugdata existing stack |
| HTTP client | cpp-httplib (header-only) | Native SSE; juce::URL lacks streaming |
| JSON | nlohmann/json (header-only) | Zero build complexity |
| Scripting | pd-lua (Lua 5.4) | Existing pd-lua integration |
| Build | CMake 3.21+ | Already in use |
| LLM backend | llama-server / Ollama | External — OpenAI-compat API only |
| Persistence | Filesystem JSON | `~/.repente-pd/` |
| CI | GitHub Actions matrix | Win/Mac/Linux from Phase 1 |

---

## Data Model

| Entity | Key Fields | Relationships |
|---|---|---|
| Session | id, canvas_path, created, last_active | has many Messages, Snapshots |
| Message | id, session_id, role, content, timestamp | belongs to Session |
| Snapshot | id, session_id, timestamp, pd_content | belongs to Session |
| Config | repente_url, model_name, canvas_mode | singleton |

**Canvas serialization:** `.pd` literal, subgraph-selected by default, full canvas optional. Object identity via auto-names (`osc_1`, `delay_2`), also addressable by `#index` or `$last`.

**Filesystem layout:**
```
~/.repente-pd/
├── config.json
└── sessions/<id>/{meta.json, messages.jsonl, snapshots/}
```

---

## API Surface

**Endpoint:** `POST {repente_url}/v1/chat/completions` (OpenAI-compatible)
**Transport:** SSE streaming → incremental `.pd` parser → Executor
**Auth:** none (local HTTP); remote URL triggers privacy warning

**pd Parser pipeline:** detect `#N canvas` → parse `#X obj` / `#X connect` lines → call `pd::Patch` API → tolerant of truncation.

---

## UI/UX

### Prompt Bar

Single **Prompt Bar** at bottom of canvas. No separate panels per engine.

**Structure (top to bottom):**
1. **Output Area** — scrollable history, collapsible to zero height. Shows Repente streaming tokens, pd-script command results, errors, and status messages. Preserves canvas real estate when collapsed.
2. **Input field** — text input with status indicator (red/green dot = server alive/dead), history navigation (↑/↓), autocomplete for `/pds` commands.

### Canvas Update Behavior

| Source | Canvas Update Timing |
|---|---|
| pd-script command | Immediate — each command is atomic, canvas updates on execution |
| Repente generation | Atomic on `[DONE]` — streaming tokens appear in Output Area only; full parsed patch applied to canvas when stream completes |

### Command Catalog

| Input | Routes to | Phase |
|---|---|---|
| `<free text>` | Repente | 3 |
| `/pds <cmd>` | pd-script engine | 2 |
| `/rep <prompt>` | Repente (explicit) | 3 |
| `/help` | inline help | 1 |
| `/clear` | clear output history | 2 |
| `/config` | server config | 3 |
| `/canvas [sub\|full]` | serialization scope | 4 |
| `/sessions` | session browser | 4 |

### pd-script Commands

```
# Base commands
create <obj> <x> <y>
connect <src> <outlet> <dst> <inlet>
delete <obj>
list
move <obj> <x> <y>
lua <expr>

# Sugar (pre-processed to base commands before Executor)
connect osc_1 dac_1              # outlet 0 → inlet 0
connect osc_1:1 filter_1         # explicit outlet, default inlet
connect osc_1 filter_1:2         # default outlet, explicit inlet
connect osc_1:1 filter_1:2       # both explicit
create osc~ 100 100 -> dac_1     # create + auto-connect (outlet 0 → inlet 0)
chain osc_1 filter_1 dac_1       # chain via outlet 0 → inlet 0
connect #0 #1                    # by creation index
connect $last dac_1              # last created object
delete osc_1 filter_1            # multi-target delete
```

---

## Integration Points

| Integration | Protocol | Direction | Notes |
|---|---|---|---|
| Repente (llama-server) | HTTP/SSE | out | localhost:8080 |
| Ollama | HTTP/SSE | out | localhost:11434 |
| pd-lua | in-process | bidirectional | Lua 5.4 |
| plugdata canvas | in-process | bidirectional | `pd::Patch`, sys_lock |

**Server config flow:** auto-detect both ports → found: green dot in Prompt Bar; not found: lightweight wizard → persist `~/.repente-pd/config.json`.

---

## Implementation Phases

| Phase | Name | Duration | Outcome |
|---|---|---|---|
| 1 | Foundation | 1-2w | Fork builds on Win/Mac/Linux; Prompt Bar renders; CI green |
| 2 | pd-script REPL | 2-3w | pd-script standalone value — no LLM needed |
| 3 | Repente Bridge | 3-4w | Text → canvas → audio. **Demo-ready.** |
| 4 | Bidirectionality + Analysis | 2-3w | Iterative LLM conversation about patch |
| 5 | Tier 2 + V1.0 Polish | 2w | Ollama support; public release |

**Backlog (post-V1):** live coding latency, PatchBox/Tier 3, multimodal loop (C3 Tríade), SC/MAX cross-language, Style Transfer.

---

## Design Decisions

1. **Repente as HTTP service:** inference outside plugdata process; switching tier = switching URL
2. **pd-script ≠ LLM interlanguage:** Repente speaks native `.pd`; pd-script is parallel user DSL; both converge at Executor only
3. **Pure Data only (MVP):** Repente v0.3 vanilla-only; SC/MAX deferred to V2
4. **Session persistence in `~/.repente-pd/`:** not embedded in `.pd` — preserves PD format
5. **Single unified Prompt Bar:** one input, namespaced commands
6. **GGUF Q4_K_M quantization:** only quantize with 100% cross-platform consistency (whitepaper)
7. **Tier 1 as MVP persona:** dogfood-first with P1 (Bidu, RTX 5070)
8. **SSE over WebSocket:** simpler, llama-server native, half-duplex sufficient
9. **Strict thread isolation:** HTTP in juce::Thread; canvas mutations in message thread only
10. **cpp-httplib:** native SSE support over juce::URL
11. **`.pd` literal canvas format:** Repente trained on it; no translation layer
12. **Subgraph serialization default:** token budget control
13. **Auto-names for object identity:** `osc_1`, `delay_2` — addressable across prompts
14. **OSS from day 1:** GPL requirement from plugdata fork
15. **pd-script sugar pre-processed to base commands:** Executor has no sugar-awareness
16. **Terminal-style Output Area above input:** scrollable, collapsible — no side panel, preserves canvas real estate
17. **Canvas updates atomic on `[DONE]` for Repente, immediate for pd-script:** streaming tokens show in Output Area only; full patch applied once stream completes

---

## Open Questions

None — all resolved during SEED session (2026-04-30).

---

## References

- `docs/00_Arquitetura_Consolidada.md` — plugdata internal architecture
- `docs/01_CommandInput_Estudo.md` — existing command system
- `docs/02_API_Patch_Estudo.md` — `pd::Patch` API
- `docs/03_Sistema_Mensagens_PD_Estudo.md` — PD message passing
- `docs/04_Integracao_pdlua_Estudo.md` — pd-lua integration
- `repente-whitepaper v3.1` — benchmarks, Battery B/F, quantization
- `01-project-vision.md` — Tríade Convergente, long-term vision
- plugdata: https://github.com/plugdata-team/plugdata
- llama.cpp: https://github.com/ggerganov/llama.cpp

---

*Last updated: 2026-04-30*
