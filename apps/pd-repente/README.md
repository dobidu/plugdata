# pd-repente

> Describe what you want to hear. A working Pure Data patch appears on the canvas and plays immediately.

[![CI](https://github.com/dobidu/plugdata/actions/workflows/cmake.yml/badge.svg)](https://github.com/dobidu/plugdata/actions)
[![License: GPL](https://img.shields.io/badge/license-GPL-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Win%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)]()

---

## What it is

`pd-repente` is a fork of [plugdata](https://github.com/plugdata-team/plugdata) with an integrated LLM composition assistant. You type a musical idea in plain language — "a filtered noise burst with slow attack" — and a valid Pure Data patch is generated, placed on the canvas, and starts running. No switching windows, no copy-pasting, no prompt engineering. The patch is the output.

The LLM is context-aware: every request includes the current canvas state as a system message in pd-file format, so the model knows what is already on the canvas before generating anything new. You can build on existing patches incrementally, ask questions about them, or generate from scratch.

`pd-repente` is local-first. It works with any OpenAI-compatible server — Ollama, llama-server, LM Studio, or the OpenAI API — and defaults to `localhost`. No data leaves your machine unless you explicitly point it at a remote endpoint.

---

## Features

- **Natural language → patch** — free text sent to LLM, response auto-detected and executed (pd patch, pds commands, or Lua)
- **Context-aware generation** — canvas serialized to pd-file format and injected as system message on every request
- **Analysis mode** — `/analyze <question>` asks the LLM about the current patch and returns plain text; nothing is executed on the canvas
- **Merge mode** — toggle in the prompt bar; generated patches merge into the current canvas instead of opening a new tab
- **pd-script REPL** — `/pds create / connect / delete / move / list` directly manipulate canvas objects
- **Sugar syntax** — `@osc~`, `~filter~`, `-> dac~`, `$last` shortcuts for fast patching
- **Lua scripting** — `{ }` blocks run inline Lua with full `pds.*` API for generative patching
- **Object Tree Panel** — sidebar shows all canvas objects grouped by type (DSP / UI / Control), including LLM-generated ones
- **Persistent config** — LLM URL, model name, and API key stored across restarts
- **Local-first** — defaults to `localhost`; works fully offline with a local model

---

## Quick Start

### Build

```bash
git clone --recursive https://github.com/dobidu/plugdata.git
cd plugdata
cmake -S . -B build -G Ninja
cmake --build build --target plugdata_standalone_Standalone -j$(nproc)
```

Binary: `build/plugdata_artefacts/Standalone/plugdata`

### Connect an LLM

```
/config url http://localhost:11434    # Ollama running locally
/config model llama3.2               # any OpenAI-compat model name
/config test                         # verify connection
```

### Make sound

```
make a sine wave at 440 Hz connected to output
```

A patch with `osc~ 440` → `dac~` appears and plays. That's it.

---

## Usage

### Free text → patch

Type any musical description. The LLM receives your text plus the current canvas state and responds with a patch, a set of pds commands, or a Lua block — `pd-repente` detects the format automatically and executes it.

```
a four-voice chord with detuned oscillators and a reverb tail
add a low-pass filter with a slow LFO on the cutoff
route everything through a soft limiter before dac~
```

New patches open in a new tab by default. Enable **merge mode** to insert objects into the current canvas instead.

### Analysis mode

Use `/analyze` when you want to ask about the patch without triggering execution. The LLM sees the full canvas context and responds in plain text — nothing is created or modified.

```
/analyze what does this patch do?
/analyze why might this be clipping?
/analyze suggest a way to add stereo width
```

### Merge mode

The **merge** toggle in the prompt bar (left side) controls where generated patches land:

- **Off** (default): each LLM patch opens in a new tab
- **On**: objects and connections are inserted into the current canvas

State persists across restarts.

### pd-script REPL

Direct canvas manipulation without the LLM:

```
/pds create osc~ 100 100
/pds create dac~ 100 200
/pds connect osc~ dac~ 0 0
/pds move osc~ 200 100
/pds delete osc~
/pds list
```

Objects created via `/pds` get auto-assigned names (`osc_1`, `dac_1`, …) used for subsequent connect/move/delete.

### Sugar syntax

Shortcuts that expand before parsing:

```
@osc~ 440          → /pds create osc~ 440
~filter~           → /pds create filter~
-> dac~            → create dac~ + auto-connect from last object
$last              → expands to the last created object name
```

### Lua scripting

Wrap any Lua expression in `{ }`. The `pds` table gives synchronous access to the REPL from Lua:

```lua
{ for i=1,4 do pds.create("osc~", i*80, 100) end }

{
  local n = pds.create("osc~", 100, 100)
  local d = pds.create("dac~", 100, 200)
  pds.connect(n, 0, d, 0)
}
```

`pd.post(msg)` logs to console. `pd.eval(cmd)` runs any REPL command string from Lua.

### Debugging canvas context

```
/canvas
```

Prints the pd-file snapshot that will be sent to the LLM as system context on the next request. Useful for verifying what the model sees.

---

## Command Reference

| Command | Description |
|---|---|
| `<free text>` | Send to LLM; canvas state injected automatically |
| `/analyze <question>` | Ask LLM about the patch — text response only, no execution |
| `/pds <cmd>` | pd-script REPL (create / connect / delete / move / list) |
| `/lua <expr>` | Run Lua expression inline |
| `/config` | Show current LLM settings |
| `/config url <url>` | Set server URL (OpenAI-compatible) |
| `/config model <name>` | Set model name |
| `/config key <key>` | Set API key (stored, masked in display) |
| `/config test` | Ping server and verify connectivity |
| `/canvas` | Print serialized canvas state (debug) |
| `/help [topic]` | Help topics: `pds` · `sugar` · `lua` · `llm` · `commands` · `builtin` |
| `/clear` | Clear console |

---

## LLM Backend Setup

`pd-repente` speaks the OpenAI Chat Completions API (`POST /v1/chat/completions`). Any compatible server works.

### Ollama (recommended for local use)

```bash
ollama serve
ollama pull llama3.2   # or any model
```

```
/config url http://localhost:11434
/config model llama3.2
```

### llama-server / LM Studio

```
/config url http://localhost:1234   # LM Studio default
/config model <loaded-model-name>
```

### OpenAI API

```
/config url https://api.openai.com
/config model gpt-4o
/config key sk-...
```

### Repente model (recommended)

The Repente model is fine-tuned specifically for Pure Data patch generation and outputs clean, runnable pd syntax by default. Available on Hugging Face — run it with Ollama or llama-server.

```
/config url http://localhost:11434
/config model repente-1
```

---

## Architecture

For contributors and developers.

### Component overview

```
User input (free text or /pds command)
        │
   PromptInput  ──  merge toggle
        │
   ┌────┴──────────────────────┐
   │                           │
PD-SCRIPT ENGINE          REPENTE BRIDGE
CommandParser              Bridge
SugarExpander         CanvasSerializer ──→ pd-file context
Executor              RepenteClient   ──→ HTTP POST /v1/chat/completions
   │                  PdParser
   │                  Executor / openPatch / PatchMerger
   └──────┬───────────┘
          │
       EXECUTOR
       pd::Patch wrapper
       per-canvas registry
       message thread only
          │
    PURE DATA CANVAS (libpd)
```

### Response routing

`PdParser` auto-detects LLM response format:

| Detected format | Route | Effect |
|---|---|---|
| Starts with `#N canvas` | PD_PATCH | Open new tab (or merge via PatchMerger) |
| Lines starting with `/pds` | PDS_COMMANDS | Execute via Executor |
| Anything else | LUA_BLOCK | Run in Lua engine |

`/analyze` bypasses `PdParser` entirely — response logged as plain text.

### Threading model

- **Audio thread** — never touched by repente code
- **HTTP thread** (`std::thread`) — all `RepenteClient` network calls
- **Message thread** — all canvas mutations, via `MessageManager::callAsync`
- **Cancel token** (`shared_ptr<atomic<bool>>`) — safe shutdown of detached threads on quit

### Key design decisions

| Decision | Rationale |
|---|---|
| pd-file format for canvas context | LLMs are trained on it; no translation layer needed |
| OpenAI-compat API only | Switching model = switching URL; no binary integration |
| Per-canvas Executor registry | Object identity (`osc_1`) isolated per tab |
| SettingsFile for config | Custom keys registered in `defaultSettings`; cross-platform persistence |
| CONFIGURE_DEPENDS on cmake glob | New `Bridge/*.cpp` files auto-included without reconfigure |

---

## Roadmap

- **Phase 04** (current) — bidirectionality: context injection, analysis mode, merge mode, session persistence
- **Phase 05** — auto-layout (smart object placement via `CanvasLayouter`), Ollama auto-detect, first-launch wizard, V1.0 public release

Media, screenshots, and demo patches will be added at V1.0.

---

## Stack

| Layer | Choice |
|---|---|
| Host | C++17 + JUCE 7.x + libpd |
| HTTP client | cpp-httplib (header-only, vendored) |
| JSON | nlohmann/json (header-only, vendored) |
| Scripting | pd-lua / LuaJIT 5.4 |
| Build | CMake 3.21+ + Ninja |
| CI | GitHub Actions (Win / macOS / Linux) |

---

## License

GPL — same as plugdata. See [LICENSE](../../LICENSE).

Based on [plugdata](https://github.com/plugdata-team/plugdata) by Timothy Schoen et al.

---

*Planning and phase tracking: `.paul/` (PAUL framework)*
