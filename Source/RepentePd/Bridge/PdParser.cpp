/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "PdParser.h"

namespace RepentePd {

juce::String PdParser::stripFences(juce::String const& s)
{
    auto t = s.trim();
    if (!t.startsWith("```")) return t;
    int firstNewline = t.indexOf("\n");
    if (firstNewline < 0) return t;
    // Drop first line (``` or ```pd, ```lua, etc.) and trailing ```
    auto body = t.substring(firstNewline + 1).trimEnd();
    if (body.endsWith("```"))
        body = body.dropLastCharacters(3).trimEnd();
    return body;
}

ParsedResponse PdParser::parse(juce::String const& response)
{
    juce::String content = stripFences(response);

    if (content.containsIgnoreCase("#N canvas"))
        return { ResponseType::PD_PATCH, content };

    // Check first non-empty line for /pds prefix
    for (auto const& line : juce::StringArray::fromLines(content)) {
        auto trimmed = line.trim();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith("/pds"))
            return { ResponseType::PDS_COMMANDS, content };
        break;
    }

    return { ResponseType::LUA_BLOCK, content };
}

} // namespace RepentePd
