# Capturing snapshots of pd-repente

Figures for the repo README and for academic publication. Written after a WSL2
session hit walls that macOS does not — read § Why macOS before choosing a box.

## Why macOS

| | macOS | WSL2 (WSLg) |
|---|---|---|
| Audio | real devices | **none** — no ALSA cards, no PulseAudio, JACK absent |
| `/listen` + audible verification | works | impossible |
| Screenshot CLI | `screencapture`, built in | nothing installed but `xwd`; needs ImageMagick/grim/scrot, and WSLg's compositor may refuse wlr-screencopy |
| Keystroke automation | `osascript` + System Events, built in | needs `xdotool`, XWayland-dependent |
| Pixel density | 2× (Retina) — good for print | 1× |

The audio row is the decisive one. `/listen` captures 3 s from the audio thread
and runs an FFT over it; with no device the capture is silence and the spectral
figure is meaningless. Anything illustrating the C3 layer of the Tríade has to
run on a machine with working audio.

## Model

pd-repente sends **no instructional system prompt** — only a `system` message
carrying canvas/audio context when there is any. The model is expected to be
intrinsically Pd-tuned. But the training system prompt is multi-language
("SuperCollider, Pure Data, and MAX/MSP") and the corpus includes SC, so a bare
GGUF import answers in SuperCollider a good share of the time.

Fix by baking the Pd constraint into the Ollama model — see
`../scripts/Modelfile.repente-pd`:

```sh
ollama create repente:v0.3-pd -f scripts/Modelfile.repente-pd
```

Verify before shooting anything:

```sh
curl -s http://localhost:11434/v1/chat/completions \
  -H 'Content-Type: application/json' \
  -d '{"model":"repente:v0.3-pd","messages":[{"role":"user","content":"create a sine wave at 440 Hz connected to dac~"}],"max_tokens":600}' \
  | python3 -c "import sys,json;c=json.load(sys.stdin)['choices'][0]['message']['content'];print(c[:300]);print('HAS #N canvas:', '#N canvas' in c)"
```

### v0.7 is not directly servable

`training_dump/v7/checkpoints/repente-v0.7/final/` holds a **LoRA adapter**
(`adapter_model.safetensors`, r=32 / α=64), and its `adapter_config.json` records
the base as `/workspace/models/base` — a container path. To serve it:

1. Identify and fetch the base model in HF format (the quantized GGUF will not do
   — merging needs unquantized weights).
2. `peft` merge: load base, `PeftModel.from_pretrained(..., adapter)`,
   `merge_and_unload()`, save.
3. Convert with llama.cpp `convert_hf_to_gguf.py`, then quantize to Q4_K_M.
4. `ollama create repente:v0.7 -f` a Modelfile pointing at the result, reusing the
   SYSTEM block from `Modelfile.repente-pd`.

v0.3 output is visibly rough — incoherent object choices, frequent truncation. For
README figures that is tolerable; for publication figures, build v0.7 first.

## Automated run

```sh
./apps/pd-repente/scripts/snapshot-session.sh out/snapshots
```

macOS only. Grant the launching terminal both **Screen & System Audio Recording**
and **Accessibility** first — without Accessibility the synthesized keystrokes go
nowhere silently, and without Screen Recording every PNG is an empty desktop.

The script pins the window to a fixed 1600×1000 rect so the whole set is frame
identical, configures the app, discards a warm-up generation (the first request
pays model load), then walks Battery B capturing generated + `/arrange` pairs,
plus the no-patch, truncation, and `/listen` cases.

**Review every frame by hand.** Model output varies run to run and a figure set is
only as good as its worst frame.

## Manual run

Configure, in the prompt bar:

```
/config url http://localhost:11434
/config model repente:v0.3-pd
/config maxtokens 1536
/config autoplace on
/config test
```

`/config test` prints `connected — models available: N`. `maxtokens 1536` matters:
v0.3 truncates near 512 by default. Send one prompt and throw the result away
before capturing, so model-load latency is not in frame.

### Prompts

v0.3 is small — stay near the training distribution. Short, canonical DSP. These
are Battery B from the whitepaper, so they double as reproducible figures:

```
create a sine wave at 440 Hz connected to dac~
noise~ through a lop~ lowpass into dac~
osc~ with an LFO modulating amplitude into dac~
metro 500 driving a counter and sel for a rhythm pattern
osc~ into delwrite~ and delread~ for a delay line
```

Battery F, one canvas across four turns — shows multi-turn context carrying:

```
create a pad sound with osc~ and an envelope
add a drum kit with noise~ snare and osc~ kick
add a metro-driven rhythm pattern
combine the pad, drums and pattern into one patch
```

Avoid open-ended prompts ("build an ambient generative system"). v0.3 drifts and
truncates — a granular-cloud request came back as `fft~`/`rifft~` soup.

## Shot list — README

| Shot | How |
|---|---|
| Hero: patch + prompt bar + teal console | Battery B #1, capture whole window |
| Prompt bar closeup | Type, don't send; crop to the bottom bar |
| Before/after generation | Empty canvas, then post-generation, same window rect |
| Object tree sidebar | Open the panel, click a row — canvas selection syncs both ways |
| `/arrange` | Capture, run `/arrange top-down 80`, capture again |
| REPL without a model | `/pds create osc~ 100 100`, `/pds create dac~ 100 200`, `/pds connect osc_1 dac_1 0 0` |
| `/analyze` | `/analyze what does this patch do?` — text only, no execution |

## Shot list — publication

Each maps to an architectural claim:

- **C1→C2 mapping.** Prompt and resulting canvas together, console visible.
- **Bidirectionality.** Generate, select a subset, then `add a lowpass filter after
  the oscillator` — the canvas goes back as context.
- **Provider abstraction.** `/config` output plus `/config preset list`. Shows
  tier-portability with no code change.
- **Graceful degradation.** A prose answer prints
  `repente: no patch in response — the model answered in prose` and shows the text.
  Pair with a pre-fix screenshot (`Lua error: unexpected symbol near 'not'`) for a
  before/after.
- **Truncation handling.** `/config maxtokens 200`, then Battery B #5 → 
  `patch fragment without a '#N canvas' header — generation was likely truncated`.
  Reset to 1536 after.
- **C3 multimodal.** `/listen` after a generation — spectral analysis injected as
  context. Requires audio; macOS only.

## Mechanics

- Capture: `screencapture -x -R0,0,1600,1000 out.png`, or `Cmd+Shift+4` then space
  for a window (`-o` / no shadow keeps figures tight).
- Keep one window size across the whole set — resizing between shots makes a
  figure set look sloppy. ≥1600 px wide for publication.
- Light theme reproduces better in print, dark reads better on GitHub. Don't mix
  within a set.
- `/history clear` and clear the console between shots so only the relevant
  exchange is visible.
