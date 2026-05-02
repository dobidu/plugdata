---
phase: 04-bidirectionality
plan: 03
subsystem: llm
tags: [conversation-history, multi-turn, persistence, settingsfile, json]

requires:
  - phase: 04-02
    provides: Bridge + RepenteClient wiring, /analyze mode, SettingsFile keys

provides:
  - Multi-turn conversation history in Bridge (20-turn rolling window)
  - RepenteClient accepts full messages array (system + history + user)
  - History serialized to SettingsFile; opt-in persistence via /config history on|off
  - /history and /history clear commands
  - Startup focus on prompt bar
  - /config history on|off command

affects: [05-llm-ux, any future streaming or sessions work]

tech-stack:
  added: []
  patterns:
    - Bridge owns message construction (RepenteClient is transport-only)
    - JUCE JSON (DynamicObject/juce::JSON) for history serialization (no new dep)
    - Persistence opt-in by default (repente_persist_history = false)

key-files:
  created: []
  modified:
    - Source/RepentePd/Bridge/RepenteClient.h
    - Source/RepentePd/Bridge/RepenteClient.cpp
    - Source/RepentePd/Bridge/Bridge.h
    - Source/RepentePd/Bridge/Bridge.cpp
    - Source/RepentePd/UI/PromptInput.cpp
    - Source/Utility/SettingsFile.h

key-decisions:
  - "JUCE JSON instead of nlohmann for Bridge serialization — avoids new dep"
  - "History persistence off by default — user-controlled via /config history on|off"
  - "analyzeOnly: history sent as context but NOT stored — out-of-band reads, clean conversation"

patterns-established:
  - "Bridge builds full messages vector; RepenteClient is pure transport"
  - "New SettingsFile keys must be registered in defaultSettings map"

duration: ~2h
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 04 Plan 03: Session Persistence Summary

**Multi-turn LLM conversations via rolling 20-turn history; opt-in cross-session persistence via `/config history on|off`; startup focus on prompt bar.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~2h |
| Tasks | 3 + 2 scope additions |
| Files modified | 6 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Multi-turn context sent to LLM | Pass | Bridge builds [system + history + user] array each request |
| AC-2: analyzeOnly bypasses history accumulation | Pass | Early return before history append; history still sent as context |
| AC-3: History persists across restarts | Pass | saveHistory/loadHistory + SettingsFile (requires `history on`) |
| AC-4: /history clear wipes state and storage | Pass | clearHistory() clears vector + sets repente_history = "" |

## Accomplishments

- RepenteClient refactored to accept `vector<Message>` — clean transport-only interface
- Bridge accumulates user+assistant pairs capped at 20 turns (40 messages)
- JUCE-native JSON serialization (`juce::DynamicObject`, `juce::JSON::toString`) — no new dependency
- `/history` (count) and `/history clear` commands wired in PromptInput
- `/config history on|off` — persistence opt-in; turning off clears stored history
- Startup auto-focus on prompt bar via `MessageManager::callAsync`

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1-2-3: RepenteClient + Bridge + /history | `c23f70d` | Multi-turn conversation history + /history command |
| Scope: startup focus | `cad77bc` | Focus prompt bar on startup |
| Scope: /config history on\|off | `43b321a` | Toggle cross-session persistence |

Plan metadata: `62c3c2b` (plan: session persistence)

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `Source/RepentePd/Bridge/RepenteClient.h` | Modified | Added `Message` struct; replaced send() signature |
| `Source/RepentePd/Bridge/RepenteClient.cpp` | Modified | Iterates messages array for JSON body |
| `Source/RepentePd/Bridge/Bridge.h` | Modified | HistoryMessage, MAX_HISTORY_TURNS, clearHistory, historyTurnCount |
| `Source/RepentePd/Bridge/Bridge.cpp` | Modified | send() builds messages vector; saveHistory/loadHistory; serialize/deserialize |
| `Source/RepentePd/UI/PromptInput.cpp` | Modified | /history handler; /config history on\|off; /help llm updated; startup focus |
| `Source/Utility/SettingsFile.h` | Modified | Added repente_persist_history (false) and repente_history ("") keys |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| JUCE JSON for serialization | Avoids nlohmann/json in Bridge.cpp; JUCE already available | No new dep; slightly more verbose but stable |
| History persistence off by default | User-requested; avoids silent data accumulation | User must opt in with `/config history on` |
| analyzeOnly: history sent but not stored | Reads context without polluting turn history | /analyze stays out-of-band; clean UX |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Scope additions | 2 | Both user-requested; no scope creep |
| Approach change | 1 | Serialization via JUCE JSON instead of nlohmann/json |

### Approach Change

**Serialization library**
- **Planned:** nlohmann/json (already vendored)
- **Actual:** JUCE `DynamicObject` + `juce::JSON::toString`
- **Reason:** nlohmann/json not included in Bridge.cpp include path cleanly; JUCE JSON sufficient for flat key-value array
- **Impact:** None — same result, simpler include chain

### Scope Additions

1. **Startup prompt bar focus** (`cad77bc`) — user requested; `grabInputFocus()` method added to CommandInput, called via `callAsync` in PluginEditor
2. **`/config history on|off`** (`43b321a`) — user requested; persistence opt-in command with UI feedback; turning off clears stored history

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| `commandInput` private in CommandInput | Added `grabInputFocus()` public method |
| LSP false positives (clang PCH override warnings) | Verified actual cmake build was clean; LSP artifacts from stale PCH |

## Next Phase Readiness

**Ready:**
- LLM bridge fully functional: config, history, analyze, merge modes
- SettingsFile pattern established for new keys (must register in defaultSettings)
- Bridge/RepenteClient interface stable — transport-only split clean

**Concerns:**
- CanvasSerializer is top-level only (no sub-patch recursion) — noted in open items
- Battery F not yet verified end-to-end (Phase 04 overall goal)

**Blockers:** None

---
*Phase: 04-bidirectionality, Plan: 03*
*Completed: 2026-05-02*
