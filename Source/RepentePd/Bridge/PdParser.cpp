/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "PdParser.h"

namespace RepentePd {

juce::String PdParser::fenceTag(juce::String const& s)
{
    auto t = s.trim();
    if (!t.startsWith("```")) return {};
    int firstNewline = t.indexOf("\n");
    auto tag = firstNewline < 0 ? t.substring(3)
                                : t.substring(3, firstNewline);
    return tag.trim().toLowerCase();
}

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

bool PdParser::looksLikeLua(juce::String const& s)
{
    // The pds.* API is the strongest signal — it only exists in our Lua binding.
    if (s.contains("pds.")) return true;

    // Otherwise require a block opener at the start of some line plus a
    // matching `end`. Prose says "if" and "for" too, but not in this shape.
    static char const* openers[] = { "local ", "function ", "for ", "while ", "if " };
    bool hasOpener = false;
    for (auto const& line : juce::StringArray::fromLines(s)) {
        auto t = line.trim();
        for (auto const* kw : openers) {
            if (t.startsWith(kw)) { hasOpener = true; break; }
        }
        if (hasOpener) break;
    }
    if (!hasOpener) return false;

    for (auto const& line : juce::StringArray::fromLines(s)) {
        auto t = line.trim();
        if (t == "end" || t.endsWith(" end") || t.endsWith("end;")) return true;
    }
    // `local x = 1` is valid Lua with no `end` — accept a bare assignment too.
    return s.contains("local ") && s.contains("=");
}

ParsedResponse PdParser::parse(juce::String const& response)
{
    auto const tag     = fenceTag(response);
    juce::String content = stripFences(response);

    if (content.trim().isEmpty())
        return { ResponseType::NO_PATCH, content, NoPatchReason::Empty };

    if (content.containsIgnoreCase("#N canvas"))
        return { ResponseType::PD_PATCH, content, NoPatchReason::None };

    // Patch body with no header — v0.3/v0.4 truncation cuts the top line off.
    // Executing this as Lua is what produced the `unexpected symbol` errors,
    // so report it instead.
    if (content.contains("#X obj") || content.contains("#X connect")
        || content.contains("#X msg"))
        return { ResponseType::NO_PATCH, content, NoPatchReason::PatchFragment };

    // Check first non-empty line for /pds prefix
    for (auto const& line : juce::StringArray::fromLines(content)) {
        auto trimmed = line.trim();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith("/pds"))
            return { ResponseType::PDS_COMMANDS, content, NoPatchReason::None };
        break;
    }

    // Lua needs positive evidence: an explicit ```lua fence, or Lua-shaped code.
    // Without this the fallback fed prose to the interpreter.
    if (tag == "lua" || looksLikeLua(content))
        return { ResponseType::LUA_BLOCK, content, NoPatchReason::None };

    return { ResponseType::NO_PATCH, content, NoPatchReason::Prose };
}

juce::String PdParser::describe(NoPatchReason reason)
{
    switch (reason) {
        case NoPatchReason::Prose:
            return "no patch in response — the model answered in prose";
        case NoPatchReason::PatchFragment:
            return "patch fragment without a '#N canvas' header — generation was likely truncated";
        case NoPatchReason::Empty:
            return "empty response";
        case NoPatchReason::None:
        default:
            return {};
    }
}

} // namespace RepentePd
