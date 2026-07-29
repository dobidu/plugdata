/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>

namespace RepentePd {

enum class ResponseType { PD_PATCH, LUA_BLOCK, PDS_COMMANDS, NO_PATCH };

// Why a response was classified NO_PATCH. Drives the console message.
enum class NoPatchReason {
    None,
    Prose,          // plain text — the model answered instead of emitting a patch
    PatchFragment,  // #X lines present but no #N canvas header — truncated generation
    InvalidLua,     // Lua-shaped, but luaL_loadstring rejected it (often another language)
    Empty           // nothing left after stripping fences
};

struct ParsedResponse {
    ResponseType type;
    juce::String content; // stripped of markdown fences
    NoPatchReason reason = NoPatchReason::None;
    // Non-empty lines dropped by extractPatch() because they sat outside the
    // patch body. Non-zero means the model padded its reply; Bridge logs it so
    // a silently-truncated response is still visible.
    int discardedLines = 0;
};

class PdParser {
public:
    // Detects format and strips markdown code fences.
    // Never throws — returns NO_PATCH when nothing executable is recognized,
    // so prose is reported rather than fed to the Lua engine.
    static ParsedResponse parse(juce::String const& response);

    // Human-readable explanation for a NO_PATCH result.
    static juce::String describe(NoPatchReason reason);

private:
    // Returns the fence info tag ("pd", "lua", ...) or an empty string.
    static juce::String fenceTag(juce::String const& s);
    static juce::String stripFences(juce::String const& s);
    // Slices out just the Pd records, discarding anything before the "#N canvas"
    // header or after the last record. Sets outDiscarded to the number of
    // non-empty lines dropped.
    static juce::String extractPatch(juce::String const& s, int& outDiscarded);
    static bool looksLikeLua(juce::String const& s);
    static bool isValidLua(juce::String const& s);
};

} // namespace RepentePd
