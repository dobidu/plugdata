# pd-repente

> Describe what you want to hear. A working Pure Data patch appears on the canvas and plays immediately.

[![CI](https://github.com/dobidu/plugdata/actions/workflows/cmake.yml/badge.svg)](https://github.com/dobidu/plugdata/actions)
[![License: GPL](https://img.shields.io/badge/license-GPL-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Win%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)]()

---

## What it is

`pd-repente` is a fork of [plugdata](https://github.com/plugdata-team/plugdata) with an integrated LLM composition assistant. You type a musical idea in plain language — "a filtered noise burst with slow attack" — and a valid Pure Data patch is generated, placed on the canvas, and starts running. No switching windows, no copy-pasting, no prompt engineering. The patch is the output.

The LLM is context-aware: every request includes the current canvas state as a system message in pd-file format, so the model knows what is already on the canvas before generating anything new. You can build on existing patches incrementally, ask questions about them, or generate from scratch.

`pd-repente` is **local-first**. It works with any OpenAI-compatible server — Ollama, llama-server, LM Studio, or the OpenAI API — and defaults to `localhost`. No data leaves your machine unless you explicitly point it at a remote endpoint. Ollama is auto-detected on first launch with no configuration required.

### The C3 Tríade

`pd-repente` implements a full multimodal feedback loop:

```
generate → play → /listen → analyze → refine
```

After generating a patch and hearing it play, type `/listen make the bass heavier` — the tool captures three seconds of audio output, analyzes the spectrum (frequency bands + peak frequencies), and sends both the canvas state and the spectral analysis as context to the LLM. The model hears what it built, not just what it specified.

---

## Features

- **Natural language → patch** — free text sent to LLM; response auto-detected and executed (pd patch, pds commands, or Lua)
- **Multimodal /listen** — captures 3s of audio output, analyzes spectrum, injects canvas + spectral summary into next LLM request
- **Context-aware generation** — canvas serialized to pd-file format and injected as system message on every request
- **Multi-turn conversation** — 20-turn rolling history; LLM builds on previous exchanges
- **Analysis mode** — `/analyze <question>` returns plain text; nothing is executed on the canvas
- **Merge mode** — toggle in the prompt bar; generated patches merge into the current canvas instead of opening a new tab
- **Auto-placement** — objects created via REPL or LLM are placed at available canvas positions; no coordinate guessing
- **Signal-flow layout** — `/arrange` reflows objects by topology (sources → processors → sinks)
- **pd-script REPL** — `/pds create / connect / delete / move / list` directly manipulate canvas objects
- **Sugar syntax** — `@osc~`, `~filter~`, `-> dac~`, `$last` shortcuts for fast patching
- **Lua scripting** — `{ }` blocks run inline Lua with full `pds.*` API for generative patching
- **Object Tree Panel** — sidebar shows all canvas objects grouped by type (DSP / UI / Control); click to select on canvas; canvas selection syncs back to tree
- **Ollama auto-detect** — first launch pings `localhost:11434` and `localhost:7860` automatically; no `/config` needed if Ollama is running
- **Persistent config** — LLM URL, model name, API key, and history preferences stored across restarts
- **Local-first** — defaults to `localhost`; fully offline with a local model

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

If Ollama is running, it is detected automatically on first launch. To configure manually:

```
/config url http://localhost:11434    # Ollama running locally
/config model llama3.2               # any OpenAI-compat model name
/config test                         # verify connection
```

### Make sound

```
make a sine wave at 440 Hz connected to output
```

A patch with `osc~ 440` → `dac~` appears and plays.

### Listen and refine

```
/listen make it warmer
```

`pd-repente` captures three seconds of audio, analyzes the spectrum, and sends the canvas state + spectral summary to the LLM with your prompt.

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

### /listen — multimodal loop

`/listen [prompt]` starts a three-second audio capture from the current output. When capture completes, the spectral analysis (5 frequency bands: sub / low / mid / high-mid / high, plus top-3 peak frequencies in Hz) is appended to the canvas context and sent to the LLM with your prompt.

```
/listen                         # default: "I just heard the audio output. What do you observe and suggest?"
/listen the bass is too muddy   # custom prompt
/listen add more high-end sparkle
```

The LLM sees both the patch structure (from the canvas) and what it sounds like (from the spectral snapshot) when generating its response.

During the capture window, the console shows:
```
repente: listening (3s)...
repente: analyzing audio...
repente: thinking...
```

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

### Object auto-placement and arrangement

Objects are placed automatically at available canvas positions — no need to specify coordinates with `/pds create`.

```
/pds create osc~            # placed at next available slot
/pds create dac~            # placed below osc~
```

After building a patch, `/arrange` reflows all objects by signal flow topology: sources (oscillators, noise~) at the top, processors (filters, effects) in the middle, sinks (dac~, throw~) at the bottom.

```
/arrange
```

### Conversation history

`pd-repente` maintains a rolling 20-turn conversation history. The LLM builds on previous exchanges within a session.

```
/history                    # show turn count
/history clear              # wipe history
/config history on          # persist history across restarts
/config history off         # session-only (default)
```

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

Objects created via `/pds` get auto-assigned names (`osc_1`, `dac_1`, …) used for subsequent connect/move/delete. Omit coordinates to use auto-placement.

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
| `/listen [prompt]` | Capture 3s audio → spectral analysis → LLM with canvas + spectrum context |
| `/analyze <question>` | Ask LLM about the patch — text response only, no execution |
| `/arrange` | Reflow objects by signal-flow topology (sources → processors → sinks) |
| `/pds <cmd>` | pd-script REPL (create / connect / delete / move / list) |
| `/lua <expr>` | Run Lua expression inline |
| `/history` | Show conversation turn count |
| `/history clear` | Wipe conversation history |
| `/config` | Show current LLM settings |
| `/config url <url>` | Set server URL (OpenAI-compatible) |
| `/config model <name>` | Set model name |
| `/config key <key>` | Set API key (stored, masked in display) |
| `/config test` | Ping server and verify connectivity |
| `/config history on\|off` | Persist conversation history across restarts |
| `/config autoplace on\|off` | Auto-placement (on) vs. LLM-specified coordinates (off) |
| `/canvas` | Print serialized canvas state (debug) |
| `/help [topic]` | Help topics: `pds` · `sugar` · `lua` · `llm` · `commands` · `builtin` |
| `/clear` | Clear console |

---

## LLM Backend Setup

`pd-repente` speaks the OpenAI Chat Completions API (`POST /v1/chat/completions`). Any compatible server works.

### Ollama (recommended for local use)

Ollama running on `localhost:11434` is auto-detected on first launch — no configuration needed.

```bash
ollama serve
ollama pull llama3.2   # or any model
```

To override:
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

> **Privacy:** A warning is shown any time a non-localhost URL is configured. Canvas patch data will be sent to that server.

### Repente model (recommended)

The Repente model is fine-tuned specifically for Pure Data patch generation and outputs clean, runnable pd syntax by default. Available on Hugging Face — run it with Ollama or llama-server.

```
/config url http://localhost:11434
/config model repente-1
```

---

## Architecture

### Component overview

```
User input (free text, /pds, /listen, ...)
        │
   PromptInput  ──  merge toggle
        │
   ┌────┴────────────────────────────────┐
   │                                     │
PD-SCRIPT ENGINE                   REPENTE BRIDGE
CommandParser                       Bridge
SugarExpander                  CanvasSerializer ──→ pd-file context
Executor                       SpectralAnalyzer ──→ spectral context (/listen)
   │                           RepenteClient   ──→ HTTP POST /v1/chat/completions
   │                           PdParser
   │                           Executor / openPatch / PatchMerger
   │
AUDIO CAPTURE (AudioCapture)
processBlock tap (wait-free)
startCapture / feedAudio / takeCapture
        │
   SpectralAnalyzer::analyze()
   FFT (2048pt Hann, 50% overlap)
   5 bands + top-3 peaks → text
        │
   Bridge::send(..., audioContext)
   system message: canvas + "\n" + spectral
```

### /listen flow

```
/listen prompt
    │
    ├─ startAudioCapture(3s)       ← PluginProcessor.startAudioCapture()
    │   └─ AudioCapture::startCapture()
    │       └─ processBlock feeds AudioCapture::feedAudio() [audio thread, wait-free]
    │
    ├─ (3.2s delay via juce::Timer::callAfterDelay)
    │
    ├─ takeCapture()               ← returns juce::AudioBuffer<float>
    ├─ SpectralAnalyzer::analyze() ← multi-frame FFT
    ├─ SpectralAnalyzer::format()  ← "spectral: sub=X.XX low=X.XX ... | peaks: NNNHz"
    └─ Bridge::send(prompt, false, {}, spectralText)
         └─ system message = canvas + "\n" + spectralText
```

### Response routing

`PdParser` auto-detects LLM response format:

| Detected format | Route | Effect |
|---|---|---|
| Starts with `#N canvas` | PD_PATCH | Open new tab (or merge via PatchMerger) |
| Lines starting with `/pds` | PDS_COMMANDS | Execute via Executor |
| Anything else | LUA_BLOCK | Run in Lua engine |

`/analyze` and `/listen` responses bypass `PdParser` for analysis display — but `/listen` with a generation prompt goes through the normal routing.

### Threading model

| Thread | Role |
|---|---|
| **Audio thread** | `processBlock` → `AudioCapture::feedAudio()` — wait-free atomic write only; no mutex |
| **HTTP thread** (`std::thread`) | All `RepenteClient` network calls |
| **Message thread** | All canvas mutations; all Bridge/PromptInput logic; `Timer::callAfterDelay` callbacks |
| **Cancel token** (`shared_ptr<atomic<bool>>`) | Safe shutdown of detached HTTP threads on quit |

### Key design decisions

| Decision | Rationale |
|---|---|
| pd-file format for canvas context | LLMs are trained on it; no translation layer needed |
| OpenAI-compat API only | Switching model = switching URL; no binary integration |
| Per-canvas Executor registry | Object identity (`osc_1`) isolated per tab |
| AudioCapture: push from audio thread (wait-free), pull from message thread | Audio thread never blocks; no mutex in hot path |
| audioContext appended to canvas context in single system message | One system message; LLM sees canvas + spectral in one block |
| `juce::Timer::callAfterDelay` for async capture wait | Fires on message thread; no extra threading needed |
| SpectralAnalyzer: stateless static methods, FFT per call | No state to manage; `/listen` is not real-time |
| History persistence off by default | Opt-in; user controls what leaves the session |
| CONFIGURE_DEPENDS on cmake glob | New `Bridge/*.cpp` files auto-included without reconfigure |

---

## Roadmap

| Milestone | Status |
|---|---|
| **M1: MVP** — Foundation, pd-script REPL, Repente Bridge | ✅ Complete |
| **M2: Full UX / V1.0** — Context injection, analysis, merge mode, auto-placement, /arrange, V1.0 polish | ✅ Complete |
| **M3: Multimodal Loop** — AudioCapture, SpectralAnalyzer, /listen C3 Tríade | ✅ Complete |
| **Next** — Style Transfer Sonoro, live coding latency, Tier 3 (PatchBox / Orange Pi) | Backlog |

---

## Stack

| Layer | Choice |
|---|---|
| Host | C++17 + JUCE 7.x + libpd |
| HTTP client | cpp-httplib (header-only, vendored) |
| JSON | nlohmann/json (header-only, vendored) |
| Scripting | pd-lua / LuaJIT 5.4 |
| DSP analysis | juce::dsp::FFT (JUCE DSP module) |
| Build | CMake 3.21+ + Ninja |
| CI | GitHub Actions (Win / macOS / Linux) |

---

## License

GPL — same as plugdata. See [LICENSE](../../LICENSE).

Based on [plugdata](https://github.com/plugdata-team/plugdata) by Timothy Schoen et al.

---

*Planning and phase tracking: `.paul/` (PAUL framework)*
