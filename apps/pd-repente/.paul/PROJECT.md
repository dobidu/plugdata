# pd-repente

## Core Value
plugdata + Repente: compose Pure Data patches by describing what you want to hear.

## Description
plugdata fork integrating Repente LLM as local-first musical composition assistant.
Natural language → valid Pure Data patch on canvas, plays immediately.

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
