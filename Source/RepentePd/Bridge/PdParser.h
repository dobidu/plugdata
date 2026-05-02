/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>

namespace RepentePd {

enum class ResponseType { PD_PATCH, LUA_BLOCK, PDS_COMMANDS };

struct ParsedResponse {
    ResponseType type;
    juce::String content; // stripped of markdown fences
};

class PdParser {
public:
    // Detects format and strips markdown code fences.
    // Never throws — returns LUA_BLOCK as fallback.
    static ParsedResponse parse(juce::String const& response);

private:
    static juce::String stripFences(juce::String const& s);
};

} // namespace RepentePd
