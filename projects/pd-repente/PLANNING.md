# pd-repente

> plugdata + Repente: compose Pure Data patches by describing what you want to hear.

**Created:** 2026-04-30
**Type:** Application
**Stack:** C++17 + JUCE 7.x + libpd + cpp-httplib + nlohmann/json + CMake
**Skill Loadout:** PAUL (required), AEGIS (recommended post-Phase 3), Caveman (Phases 3/5/debug)
**Quality Gates:** JUCE UnitTests, Battery B/F benchmarks, cross-platform CI (Win/Mac/Linux)

---

## Problem Statement

Pure Data is powerful but has a steep learning curve — building patches requires knowing object names, connection syntax, and signal flow simultaneously. `pd-repente` removes that barrier: a local LLM (Repente) converts natural language descriptions directly into valid Pure Data patches that appear on the canvas and play immediately.

**Audience:** Sound designers, composers, and researchers who know what they want to hear but want to iterate faster than manual patching allows. Primary persona: Bidu (P1) — RTX 5070, deep PD knowledge, rapid DSP iteration. Secondary: composition students (P3, Tier 2/Ollama).

**Why build:** No existing tool runs a PD-native LLM locally. Chat-LLMs describe code; pd-repente *executes* — the patch appears on the canvas and sounds. Repente already generates 100% syntactically valid vanilla PD; pd-repente is the host integration that didn't exist.

**Not in scope (MVP):** live coding latency optimization, Max/MSP/SuperCollider, embedded/PatchBox targets, multi-user, cloud inference.

---

## Tech Stack

| Layer | Choice | Rationale |
|---|---|---|
| Host | C++17 + JUCE 7.x + libpd | plugdata existing stack — no change |
| HTTP client | cpp-httplib (header-only) | Native SSE support; juce::URL lacks streaming |
| JSON | nlohmann/json (header-only) | Header-only = zero build complexity |
| Scripting | pd-lua (Lua 5.4) | pd-script Lua path via existing pd-lua integration |
| Build | CMake 3.21+ | Already in use by plugdata |
| LLM backend | Repente via llama-server / Ollama | External — not built, only consumed |
| Persistence | Filesystem JSON (~/.repente-pd/) | Sessions transcend individual patch files |
| CI | GitHub Actions matrix | Win/Mac/Linux from Phase 1 |

**Cross-platform constraint:** Windows (MSVC), macOS (Xcode/Clang), Linux (gcc/clang). Every new dependency must build on all three.

---

## Data Model

### Entities

| Entity | Key Fields | Relationships |
|---|---|---|
| Session | id, canvas_path, created, last_active | has many Messages, has many Snapshots |
| Message | id, session_id, role, content, timestamp, canvas_diff? | belongs to Session |
| Snapshot | id, session_id, timestamp, pd_content | belongs to Session |
| Config | repente_url, model_name, canvas_mode (sub/full) | singleton, persisted in config.json |

### Canvas Serialization
- **Format:** `.pd` literal — Repente trained on it; no translation layer
- **Granularity:** subgraph (selected objects) by default; full canvas optional with token-budget warning
- **When:** every prompt turn — context always current
- **Object identity:** auto-generated names (`osc_1`, `delay_2`) stored in session; addressable by name, `#index`, or `$last`

### Filesystem Layout
```
~/.repente-pd/
├── config.json
└── sessions/
    └── <session-id>/
        ├── meta.json
        ├── messages.jsonl
        └── snapshots/
```

---

## API Surface

**Endpoint:** `POST {repente_url}/v1/chat/completions` (OpenAI-compatible)

### Request (Generation mode)
```json
{
  "model": "repente-v0.3",
  "messages": [
    {"role": "system", "content": "<pd vanilla only system prompt>"},
    {"role": "user", "content": "<user prompt>"},
    {"role": "user", "content": "Current canvas context:\n<.pd serialized>"}
  ],
  "stream": true,
  "max_tokens": 1536,
  "temperature": 0.7
}
```

### Response
SSE stream: `data: {choices:[{delta:{content:"..."}}]}` → `data: [DONE]`

### pd Parser (output → executor)
- Detect `#N canvas` block start
- `#X obj <x> <y> <type> <args>` → `pd::Patch::createObject`
- `#X connect <src> <out> <dst> <in>` → `pd::Patch::createConnection`
- Tolerant of truncation — partial patches apply without crash

### System Prompt (MVP)
```
You are Repente, an assistant for Pure Data patch creation.
Always respond with valid Pure Data vanilla code in canvas format,
starting with #N canvas. Never use externals (ELSE, Cyclone, etc.).
When given a current canvas context, modify or extend it coherently.
```

### Auth Strategy
None — local HTTP only. Remote URL triggers privacy warning (see Security).

---

## Deployment Strategy

This is a desktop application. "Deployment" = distribution of pre-built binaries.

### Local Development
```
git clone --recursive https://github.com/{bidu}/pd-repente
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
External dependency: Repente running separately at `localhost:8080` (llama-server) or `localhost:11434` (Ollama).

### Release Distribution
- **Channel:** GitHub Releases
- **Artifacts:** pre-built binaries for Windows, macOS, Linux (from CI matrix)
- **License:** GPL (required — fork of plugdata which is GPL)
- **OSS:** public from day 1
- **Repente model:** distributed separately — user installs llama-server or Ollama independently

### Server Config (First Launch)
1. Auto-detect `localhost:8080` (llama-server), then `localhost:11434` (Ollama)
2. Found → green status indicator in Prompt Bar
3. Not found → lightweight wizard (URL field, test connection button, setup docs link)
4. Persisted to `~/.repente-pd/config.json`
5. Power users: edit config.json directly

---

## Security Considerations

No auth model — local desktop app, single user.

- **Privacy (remote URL):** if config URL is non-localhost, show persistent warning banner — canvas content will leave the machine. Dismissible per-session, re-shown on URL change.
- **Non-Repente model detection:** validate first response starts with `#N canvas`. If not → inline warning "Model does not appear to be Repente — output may not be valid Pure Data." Non-blocking.
- **Input validation (pd-script):** user commands tokenized and validated before hitting Executor. No shell exec, no eval — structured calls only to `pd::Patch` API.
- **Thread safety:** ADR-009 — HTTP in `juce::Thread`; canvas mutations in message thread via `MessageManager::callAsync`. Audio thread never touched by new code. `JUCE_ASSERT_MESSAGE_THREAD` in Executor.
- **Session data:** stored locally in `~/.repente-pd/`. No PII beyond canvas structure. No encryption needed.

---

## UI/UX Needs

Design system: JUCE Components — extends existing plugdata UI. No web framework.

### Key Views

| View | Purpose | Complexity |
|---|---|---|
| Prompt Bar | Primary interaction — text input, command dispatch, status | High — threading, streaming display |
| Streaming display | Token-by-token `.pd` output preview during generation | Medium |
| Config panel | Server URL, model name, test connection | Low |
| First-launch wizard | Auto-detect + manual setup | Low |

### Prompt Bar Detail
- Single text input field at bottom of canvas
- Status indicator (red/green dot = server alive/dead)
- History navigation (↑/↓)
- Autocomplete for `/pds` commands
- Streaming: shows tokens as they arrive; applies to canvas on `[DONE]`

### Command Catalog

| Input | Routes to | Phase |
|---|---|---|
| `<free text>` | Repente (default) | 3 |
| `/pds <cmd>` | pd-script engine | 2 |
| `/rep <prompt>` | Repente (explicit) | 3 |
| `/help` | inline help | 1 |
| `/clear` | clear chat history | 2 |
| `/config` | config panel | 3 |
| `/canvas [sub\|full]` | toggle serialization scope | 4 |
| `/sessions` | session browser | 4 |

### pd-script Command Set
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

Desktop-only. No responsive/mobile requirements.

---

## Integration Points

| Integration | Type | Direction | Auth | Notes |
|---|---|---|---|---|
| Repente (llama-server) | HTTP/SSE | outbound | none | OpenAI-compat, localhost:8080 |
| Ollama | HTTP/SSE | outbound | none | OpenAI-compat, localhost:11434 |
| pd-lua | in-process | bidirectional | none | Lua 5.4, existing integration |
| plugdata canvas (pd::Patch) | in-process | bidirectional | none | Thread-safe via sys_lock |

**Fallback if server down:** timeout + clear error in UI. Plugdata does not crash. Partial streamed content preserved.

---

## Phase Breakdown

### Phase 1: Foundation (1-2 weeks)
- **Build:** Fork plugdata → `pd-repente` repo, `Source/RepentePd/` skeleton, `PromptBar` JUCE component (render-only), GitHub Actions CI matrix (Win/Mac/Linux), baseline JUCE UnitTest
- **Testable:** App builds on all 3 OSes; Prompt Bar visible and accepts text; `ctest` passes
- **Outcome:** Stable dev environment; CI green; base for all future phases

### Phase 2: pd-script REPL (2-3 weeks)
- **Build:** `CommandParser` (tokenization + sugar pre-processor), `DirectCommands` (create/connect/delete/list/move/lua), `Executor` wrapper over `pd::Patch` (thread-safe), history (↑/↓), autocomplete, `/help`, `/clear`
- **Testable:** `/pds create osc~ 100 100` creates object; `/pds connect osc_1 dac_1` connects (sugar → base); audio thread unblocked throughout
- **Outcome:** pd-repente delivers standalone value — pd-script REPL functional without any LLM

### Phase 3: Repente Bridge — MVP Generation (3-4 weeks)
- **Build:** `RepenteClient` (cpp-httplib, OpenAI-compat), SSE streaming + incremental `.pd` parser, `PdParser` (truncation-tolerant), Bridge → Executor connection, `/config` panel, server auto-detect + first-launch wizard, non-Repente model detection warning, `/rep` command
- **Testable:** "create a granular cloud with stereo movement" → valid patch on canvas within ~5s; server offline → clear error, no crash; Battery B (5 tests: synth/sequencer/delay/drum/AM) pass end-to-end
- **Outcome:** Generation end-to-end functional. Demo-ready.

### Phase 4: Bidirectionality + Analysis (2-3 weeks)
- **Build:** Canvas serializer (subgraph default / full optional), context injection in request payload, Analysis mode (`/rep analyze` or similar), session persistence (`~/.repente-pd/sessions/`), periodic canvas snapshots, `/canvas`, `/sessions` commands, token-budget warning for large canvases
- **Testable:** "add a lowpass filter after the oscillator" modifies existing canvas coherently; Analysis mode returns sinestésica description; Battery F (pad→drums→pattern→combined) workflow completes
- **Outcome:** Real iterative conversation with LLM about the patch; context-aware generation

### Phase 5: Tier 2 + Polish — V1.0 (2 weeks)
- **Build:** Ollama auto-detect (localhost:11434), first-launch wizard polish, privacy warning for non-localhost URLs, user documentation (Repente + Ollama setup), manual smoke tests on 3 OSes
- **Testable:** Ollama + Repente model → wizard detects and configures without manual steps; privacy warning shows for remote URL; P1/P3 dogfood sessions
- **Outcome:** V1.0 public release — broader audience, documented, stable

### Backlog (post-V1)
- Live coding latency optimizations (P2 persona)
- Tier 3: PatchBox / Orange Pi (3B distilled model)
- Loop multimodal: audio render → spectral analysis → feedback (C3 of Tríade)
- Cross-language: SC/MAX when Repente reincorporates them
- Style Transfer Sonoro

---

## Skill Loadout & Quality Gates

### Skills

| Skill | When | Purpose |
|---|---|---|
| PAUL | From Phase 1 | Structured milestones, PLAN-APPLY-UNIFY cycle |
| AEGIS | Post-Phase 3 | Security + thread safety audit |
| Caveman | Phases 3, 5, debug sessions | Token reduction in repetitive implementation |

### Quality Gates

| Gate | Criterion | When |
|---|---|---|
| Cross-platform build | CI green on Win/Mac/Linux | Each phase |
| JUCE UnitTests | All pass (`ctest`) | Each phase |
| Audio thread safety | No violations (`JUCE_ASSERT_MESSAGE_THREAD`) | Phases 2+ |
| Battery B benchmarks | 5/5 end-to-end (text → canvas → audio) | Phase 3+ |
| Battery F workflow | pad→drums→pattern→combined completes | Phase 4 |
| Musical quality | Dogfood sessions P1/P3 (qualitative) | Phase 5 |

---

## Design Decisions

1. **Repente as HTTP service (ADR-001):** inference outside plugdata process via OpenAI-compat API. Switching tier = switching URL. No llama.cpp linking.
2. **pd-script ≠ LLM interlanguage (ADR-002):** Repente speaks native `.pd`. pd-script is a parallel user DSL. Both converge only at Executor.
3. **Pure Data only in MVP (ADR-003):** Repente v0.3 vanilla-only sweet spot. SC/MAX deferred to V2.
4. **Session persistence in `~/.repente-pd/` (ADR-004):** not embedded in `.pd` — preserves PD format cleanliness.
5. **Single unified Prompt Bar (ADR-005):** one bar, namespaced commands. No separate UI panels per engine.
6. **GGUF Q4_K_M as reference quantization (ADR-006):** only quantize with 100% cross-platform consistency per whitepaper.
7. **Tier 1 (RTX 5070 / M4) as MVP persona (ADR-007):** dogfood-first accelerates iteration.
8. **SSE over WebSocket (ADR-008):** simpler, llama-server native, half-duplex sufficient.
9. **Thread isolation (ADR-009):** HTTP in juce::Thread; canvas mutations in message thread only; audio thread never touched.
10. **cpp-httplib for HTTP client:** native SSE support; juce::URL lacks streaming.
11. **`.pd` literal for canvas serialization:** Repente trained on it; no translation layer; token-efficient.
12. **Subgraph serialization default:** token budget control; full canvas optional.
13. **Auto-names for object identity:** `osc_1`, `delay_2` — addressable across prompts by name, index, or `$last`.
14. **OSS from day 1:** GPL requirement from plugdata fork.
15. **pd-script sugar compiles to base commands:** parser pre-processes; Executor has no sugar-awareness.
16. **Terminal-style Output Area above input (collapsible):** scrollable history; no side panel; preserves canvas real estate. Shows Repente tokens, pd-script results, errors.
17. **Canvas updates atomic on `[DONE]` for Repente, immediate for pd-script:** streaming tokens appear in Output Area only; full parsed patch applied to canvas when stream completes. pd-script commands apply per execution (each is atomic).

---

## Open Questions

None — all questions resolved during SEED session (2026-04-30).

---

## Next Actions

- [ ] `/seed launch pd-repente` — graduate to `apps/pd-repente/` + initialize PAUL
- [ ] `/paul:plan 01-foundation` — begin Phase 1

---

## References

- `docs/00_Arquitetura_Consolidada.md` — plugdata internal architecture
- `docs/01_CommandInput_Estudo.md` — existing command system (Phase 2 base)
- `docs/02_API_Patch_Estudo.md` — `pd::Patch` API (Executor)
- `docs/03_Sistema_Mensagens_PD_Estudo.md` — PD message passing
- `docs/04_Integracao_pdlua_Estudo.md` — pd-lua integration
- `repente-whitepaper v3.1` — Repente benchmarks, Battery B/F, quantization decisions
- `01-project-vision.md` — Tríade Convergente, long-term vision
- `BRAINSTORM_repente-pd_para_SEED.md` — full ideation notes
- plugdata source: https://github.com/plugdata-team/plugdata
- llama.cpp: https://github.com/ggerganov/llama.cpp

---

*Last updated: 2026-04-30*

---

**Graduated:** 2026-04-30
**Location:** `apps/pd-repente/`
**README:** `apps/pd-repente/README.md`
