/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_core/juce_core.h>

namespace RepentePd {

enum class CommandType {
    PDS_CREATE,
    PDS_CONNECT,
    PDS_DELETE,
    PDS_LIST,
    PDS_MOVE,
    PDS_LUA,
    HELP,
    CLEAR,
    PASSTHROUGH, // natural language → Repente (Phase 3)
    UNKNOWN
};

struct CommandResult {
    CommandType     type  = CommandType::UNKNOWN;
    juce::StringArray args;
    juce::String    error;
    juce::String    raw;
};

class CommandParser {
public:
    static CommandResult parse(juce::String const& input);

private:
    static CommandResult parsePds(juce::StringArray const& tokens);
};

} // namespace RepentePd
