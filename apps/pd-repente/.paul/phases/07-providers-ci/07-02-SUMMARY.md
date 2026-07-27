---
phase: 07-providers-ci
plan: 02
completed: 2026-05-18
reconstructed: true
---

# Phase 07 Plan 02: LLM Provider Abstraction Summary

**RepenteClient split into a transport + pluggable provider layer; native Anthropic Claude API added alongside OpenAI-compatible; JSON preset system for backend switching.**

> ⚠️ Retro-documented during UNIFY on 2026-07-26. No PLAN.md exists — this work
> shipped off-roadmap after Milestone 3 closed.

## What Was Built

| File | Purpose | Lines |
|------|---------|-------|
| `Bridge/ILlmProvider.h` | Provider interface — buildBody / parseResponse / endpoint / auth header | 50 |
| `Bridge/OpenAIProvider.{h,cpp}` | OpenAI-compatible provider (Ollama, llama-server, repente server, GPT) | 106 |
| `Bridge/AnthropicProvider.{h,cpp}` | Native Anthropic Messages API: top-level `system`, `x-api-key`, `anthropic-version` | 163 |
| `Bridge/PresetLoader.{h,cpp}` | Built-in JSON presets + user overrides at `~/.config/plugdata/repente_presets.json` | 219 |
| `Bridge/RepenteClient.{h,cpp}` | Refactored to transport-only; delegates shape to the provider | ±211 |
| `UI/PromptInput.cpp` | `/config provider openai\|anthropic\|auto`, `/config preset <name>\|list\|export\|reload`, per-provider key storage | +242 |
| `Utility/SettingsFile.h` | Registered new persisted keys | ±10 |
| `Tests/RepentePdTests.h` | +315 lines — provider body-shape and preset-loader tests | +315 |

Prompt caching: the Anthropic system prompt is sent as an array of content blocks
carrying `cache_control`, rather than a bare string.

## AC Result (reconstructed)

| Criterion | Status |
|-----------|--------|
| Same prompt path works against OpenAI-compatible and Anthropic backends | Pass (unit-tested at the body-shape level) |
| `/config preset claude-opus` switches url + provider + model in one command | Pass |
| API keys stored per provider, not globally overwritten | Pass |
| User preset file overrides built-in defaults; `export` seeds it | Pass |
| Anthropic system block carries `cache_control` | Pass (`2bbf96810` updated the assertions to the array shape) |

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| Provider owns request/response shape; RepenteClient owns transport only | Same split already used for Bridge vs RepenteClient in Phase 04 — adding a provider is one file, no transport changes |
| Presets as JSON with a user-override file, not hardcoded enums | Users add backends without a rebuild |
| Native Anthropic API rather than an OpenAI-compat shim | Gives prompt caching and the correct system-block shape |

## Deviations

Scope grew past a single provider: the preset system and per-provider key storage
were not implied by "add Anthropic support" but were needed to make backend
switching usable from the prompt bar.

## Open Item Raised

Preset model IDs are previous-generation — `claude-opus-4-7`, `claude-sonnet-4-6`,
`claude-haiku-4-5-20251001`. All three are still active, and the request builder
sends no `temperature`/`top_p`, so nothing errors. Current-generation IDs are
`claude-opus-5` / `claude-sonnet-5` / `claude-haiku-4-5`. Refresh is a one-line
change per preset.

## Commits

`2deb2a78f`, `dbb1255b9`

---
*Reconstructed: 2026-07-26*
