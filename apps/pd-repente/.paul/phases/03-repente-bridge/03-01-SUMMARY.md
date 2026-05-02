---
phase: 03-repente-bridge
plan: 01
subsystem: api
tags: [cpp-httplib, nlohmann-json, http, openai-compat, networking]

requires:
  - phase: 02-pd-script-repl
    provides: Executor, pds Lua table, PromptInput free-text stub

provides:
  - RepenteClient: async HTTP POST to OpenAI-compat endpoint
  - cpp-httplib v0.16.2 vendored header
  - nlohmann/json v3.11.3 vendored header

affects: [03-02-bridge, 03-03-config]

tech-stack:
  added: [cpp-httplib v0.16.2, nlohmann/json v3.11.3]
  patterns:
    - "HTTP client included only in .cpp — keeps compile times sane"
    - "Detached thread + MessageManager::callAsync for non-blocking send"

key-files:
  created:
    - Libraries/cpp-httplib/httplib.h
    - Libraries/nlohmann/json.hpp
    - Source/RepentePd/Bridge/RepenteClient.h
    - Source/RepentePd/Bridge/RepenteClient.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "Config struct uses constructor defaults (not in-class initializers) — Clang bug with nested struct string defaults"
  - "httplib + json included only in .cpp, never in .h — compile time"
  - "send() returns bool (false if busy) rather than queuing — simplest correct behavior for Phase 03"

patterns-established:
  - "Vendored headers go in Libraries/<name>/<name>.h — single file, committed, no submodule"
  - "Async result pattern: detached std::thread + callAsync fires callback on message thread"

duration: ~30min
started: 2026-05-01T00:00:00Z
completed: 2026-05-01T00:00:00Z
---

# Phase 03 Plan 01: RepenteClient + HTTP Deps

**cpp-httplib and nlohmann/json vendored; RepenteClient sends async OpenAI-compat POST and fires result callback on JUCE message thread.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30min |
| Completed | 2026-05-01 |
| Tasks | 2 completed + 1 checkpoint approved |
| Files modified | 5 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Header deps available project-wide | Pass | Both headers in Libraries/, CMake include paths added |
| AC-2: RepenteClient::send dispatches HTTP POST | Pass | Async, callback on message thread, error strings on failure |
| AC-3: Build clean on Linux | Pass | RepenteClient.cpp compiled clean; vdpau pre-existing linker error exempt |

## Accomplishments

- `RepenteClient::send()` non-blocking: detached thread, busy-flag guard, callback via `callAsync`
- Headers included only in `.cpp` — zero impact on other translation units' compile time
- OpenSSL linked optionally via `find_package(OpenSSL QUIET)` — HTTPS works if available, plain HTTP otherwise

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1+2: deps + RepenteClient | `5a91fd9ad` | feat(03-01): RepenteClient + cpp-httplib/nlohmann deps |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Libraries/cpp-httplib/httplib.h` | Created | Vendored cpp-httplib v0.16.2 single-file header |
| `Libraries/nlohmann/json.hpp` | Created | Vendored nlohmann/json v3.11.3 single-file header |
| `Source/RepentePd/Bridge/RepenteClient.h` | Created | Config struct + send()/isBusy() public API |
| `Source/RepentePd/Bridge/RepenteClient.cpp` | Created | HTTP POST impl, JSON build/parse, thread + callAsync |
| `CMakeLists.txt` | Modified | Include paths for both headers; optional OpenSSL linkage |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Config struct uses explicit constructor, not in-class string initializers | Clang bug: nested struct in-class string defaults trigger "not yet parsed" error | Pattern for all future Config structs in RepentePd |
| send() returns false (not queue) when busy | Simplest correct behavior; Bridge (03-02) can show "busy" message | Queue deferred to Phase 04+ if needed |
| httplib + json in .cpp only | Each TU that includes the header would compile 10k+25k lines; .cpp isolation keeps incremental builds fast | All future consumers of these libs must follow same pattern |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 1 | Clang in-class initializer bug — essential fix |
| Deferred | 0 | — |

### Auto-fixed Issues

**1. Clang nested-struct in-class string initializer bug**
- Found during: Task 2 (first build)
- Issue: `juce::String url = "..."` inside nested `Config` struct triggered clang error "default member initializer not yet parsed"
- Fix: replaced in-class initializers with explicit `Config()` constructor
- Files: `RepenteClient.h`
- Verification: built clean after fix

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| Clang rejects in-class string defaults in nested structs | Moved defaults to explicit constructor body |

## Next Phase Readiness

**Ready:**
- `RepenteClient` API stable: `send(prompt, callback)` ready for 03-02 to call
- `json.hpp` available for PdParser to parse responses
- `httplib.h` available if streaming (SSE) added later

**Concerns:**
- cmake reconfigure required on each machine after new .cpp files (GLOB-based build)
- Large vendored headers (~35k lines total) increase cold build time slightly

**Blockers:** None

---
*Phase: 03-repente-bridge, Plan: 01*
*Completed: 2026-05-01*
