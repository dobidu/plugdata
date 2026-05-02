# pd-repente

> plugdata + Repente: compose Pure Data patches by describing what you want to hear.

**Stack:** C++17 · JUCE 7.x · libpd · cpp-httplib · nlohmann/json · CMake 3.21+
**License:** GPL (plugdata fork)

---

## Overview

`pd-repente` is a fork of [plugdata](https://github.com/plugdata-team/plugdata) that integrates a local-first LLM (via OpenAI-compatible API) as a musical composition assistant. Type a description of what you want to hear; a valid Pure Data patch appears on the canvas and plays immediately.

No chat interface, no copy-paste workflow. The patch is the output.

**Primary target:** sound designers and composers (P1 — Tier 1 hardware: RTX 5070 / M4+).

---

## Current State (Phase 04 in progress)

- ✅ **Phase 01** — Fork + CI matrix (Win/Mac/Linux), PromptBar renders
- ✅ **Phase 02** — pd-script REPL: `/pds` commands, sugar syntax, Lua+pds API, ObjectTreePanel
- ✅ **Phase 03** — LLM bridge: RepenteClient, PdParser, Bridge wiring, `/config` command, SettingsFile persistence
- 🔵 **Phase 04** — Bidirectionality: canvas context injection (done), full ObjectTreePanel scan (done), session persistence (upcoming)

**Demo-ready:** free text → LLM → valid pd patch on canvas, plays immediately. Tested with Ollama (local Mac M4).

---

## Architecture

```
User input (free text or /pds command)
         │
    PromptInput (JUCE Component, bottom of canvas)
         │
    ┌────┴────────────────┐
    │                     │
PD-SCRIPT ENGINE     REPENTE BRIDGE
(CommandParser +     (Bridge →
 SugarExpander →      CanvasSerializer →
 Executor)            RepenteClient HTTP →
    │                  PdParser →
    │                  Executor / openPatch)
    └────────┬──────────┘
             │
        EXECUTOR
        (pd::Patch wrapper,
         per-canvas registry,
         message thread only)
             │
       PURE DATA CANVAS (libpd)
```

**Threading model:**
- Audio thread: never touched by new code
- HTTP thread (`std::thread`): all RepenteClient calls
- Canvas mutations: message thread only via `MessageManager::callAsync`
- Cancel token (`shared_ptr<atomic<bool>>`): safe shutdown of detached threads

---

## Quick Start

### Build (Linux / macOS)

```bash
git clone --recursive https://github.com/dobidu/plugdata.git
cd plugdata
cmake -S . -B build -G Ninja
cmake --build build --target plugdata_standalone_Standalone -j$(nproc)
```

### Configure LLM server

```
/config url http://localhost:11434   # Ollama default
/config model llama3.2               # or any OpenAI-compat model
/config test                         # ping server
```

Settings persist across restarts via SettingsFile.

### Usage

```
# Free text → LLM → patch on canvas
make a simple drone with osc~ and dac~

# pd-script REPL
/pds create osc~ 100 100
/pds create dac~ 100 200
/pds connect osc~ dac~ 0 0

# Lua
{ for i=1,4 do pds.create("osc~", i*80, 100) end }

# Debug canvas context sent to LLM
/canvas

# Help
/help commands
/help llm
/help pds
```

---

## Commands

| Command | Description |
|---------|-------------|
| `<free text>` | Send to LLM bridge; canvas state injected as context |
| `/pds <cmd>` | pd-script engine (create / connect / delete / move / list) |
| `/lua <expr>` | Run Lua expression |
| `/config [url\|model\|key\|test]` | Show or set LLM server config |
| `/canvas` | Show serialized canvas state sent to LLM (debug) |
| `/help [topic]` | Help: pds · sugar · lua · llm · commands · builtin |
| `/clear` | Clear console |

### Sugar syntax

```
@osc~              → /pds create osc~
~osc~              → /pds create osc~~ (tilde suffix)
-> dac~            → create + auto-connect from last object
$last              → expands to last created object name
```

---

## LLM Bridge

**Endpoint:** `POST {repente_url}/v1/chat/completions` (OpenAI-compatible)

**Context injection:** every request includes a `role: system` message with the current canvas serialized as pd-file format — objects and connections. Empty canvas = no system message.

**Response parsing (PdParser):** auto-detects format:
- `#N canvas ...` → PD_PATCH: opens as new tab
- `/pds ...` lines → PDS_COMMANDS: executes via Executor
- anything else → LUA_BLOCK: runs in Lua engine

**Supported servers:** Ollama, llama-server, OpenAI API, any OpenAI-compat endpoint.

---

## Object Tree Panel

Shows all objects on the current canvas, grouped by type:
- **DSP** — signal-rate objects (ending in `~`)
- **UI** — GUI objects (bng, tgl, sliders, etc.)
- **Control** — message-rate objects

REPL-created objects show with their auto-assigned name: `osc_1  [osc~]`
Canvas/LLM objects show read-only: `[osc~]`

---

## Stack

| Layer | Choice | Rationale |
|---|---|---|
| Host | C++17 + JUCE 7.x + libpd | plugdata existing stack |
| HTTP client | cpp-httplib (header-only, vendored) | Simple, no build deps |
| JSON | nlohmann/json (header-only, vendored) | Zero build complexity |
| Scripting | pd-lua (Lua 5.4) | Existing pd-lua integration |
| Build | CMake 3.21+ + Ninja | Already in use |
| LLM backend | Ollama / llama-server / OpenAI | External — OpenAI-compat API |
| Settings | SettingsFile (plugdata built-in) | Persistent config, cross-platform |
| CI | GitHub Actions matrix | Win/Mac/Linux |

---

## Key Design Decisions

1. **OpenAI-compat API only** — switching model = switching URL, no binary integration
2. **pd-file format for context** — LLMs are trained on it; no translation layer
3. **Cancel token (`shared_ptr<atomic<bool>>`)** — detached HTTP threads safe on shutdown
4. **SettingsFile for config** — all custom keys must be registered in `defaultSettings` map
5. **Canvas scan for ObjectTreePanel** — reads all canvas objects + overlays REPL names
6. **CONFIGURE_DEPENDS on cmake glob** — new Bridge/*.cpp files auto-included without reconfigure
7. **Per-canvas Executor registry** — object identity (`osc_1`) isolated per tab

---

## Implementation Phases

| Phase | Status | Outcome |
|-------|--------|---------|
| 01: Foundation | ✅ Complete | Fork builds Win/Mac/Linux; CI green; PromptBar renders |
| 02: pd-script REPL | ✅ Complete | /pds + sugar + Lua+pds API + ObjectTreePanel |
| 03: Repente Bridge | ✅ Complete | Free text → LLM → canvas; /config; persistent settings |
| 04: Bidirectionality | 🔵 In progress | Canvas context injection; full canvas scan; sessions (upcoming) |
| 05: V1.0 Polish | ⬜ Planned | Ollama auto-detect; first-launch wizard; public release |

---

## References

- plugdata: https://github.com/plugdata-team/plugdata
- Ollama: https://ollama.com
- `apps/pd-repente/.paul/` — PAUL planning framework (phases, plans, state)

---

*Last updated: 2026-05-02 — Phase 04 in progress*
