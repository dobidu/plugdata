/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "PdParser.h"

extern "C" {
#include <pd-lua/luas/luajit/src/lua.h>
#include <pd-lua/luas/luajit/src/lauxlib.h>
}

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

juce::String PdParser::extractPatch(juce::String const& s, int& outDiscarded)
{
    auto const lines = juce::StringArray::fromLines(s);

    // Find the header. Prose before it is commentary, not patch data.
    int start = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].containsIgnoreCase("#N canvas")) { start = i; break; }
    }
    if (start < 0) { outDiscarded = 0; return s; }

    juce::StringArray kept;
    bool midRecord = false; // previous kept line had no terminating ';'

    for (int i = start; i < lines.size(); ++i) {
        auto const& line = lines[i];
        auto const trimmed = line.trim();

        if (midRecord) {
            // A record can wrap across lines (long #X text, #A array data), so
            // keep going regardless of what this line looks like.
            kept.add(line);
            midRecord = !trimmed.endsWith(";");
            continue;
        }

        // Between records: tolerate blank lines, but the next thing with content
        // has to be another record or the patch is over. Models like to append
        // commentary or a second language after a complete patch, and feeding
        // that to openPatch() instantiates each line as a bogus object.
        if (trimmed.isEmpty()) continue;
        if (!trimmed.startsWith("#")) break;

        kept.add(line);
        midRecord = !trimmed.endsWith(";");
    }

    int keptNonEmpty = 0;
    for (auto const& line : kept)
        if (line.trim().isNotEmpty()) ++keptNonEmpty;

    int totalNonEmpty = 0;
    for (auto const& line : lines)
        if (line.trim().isNotEmpty()) ++totalNonEmpty;

    outDiscarded = totalNonEmpty - keptNonEmpty;
    return kept.joinIntoString("\n");
}

bool PdParser::isValidLua(juce::String const& s)
{
    // Syntax-only check (luaL_loadstring never executes the chunk) so a
    // response that merely *looks* like code — SuperCollider, Max, pseudo-code
    // — but isn't valid Lua doesn't get silently misrouted into the engine.
    lua_State* L = luaL_newstate();
    if (!L) return false;
    bool const ok = luaL_loadstring(L, s.toRawUTF8()) == 0;
    lua_close(L);
    return ok;
}

ParsedResponse PdParser::parse(juce::String const& response)
{
    auto const tag     = fenceTag(response);
    juce::String content = stripFences(response);

    if (content.trim().isEmpty())
        return { ResponseType::NO_PATCH, content, NoPatchReason::Empty };

    if (content.containsIgnoreCase("#N canvas")) {
        // Keep only the patch records. A reply that is a valid patch *followed*
        // by prose or another language used to route here carrying the whole
        // string, and every trailing line became a "no such object" error.
        int discarded = 0;
        auto patch = extractPatch(content, discarded);
        return { ResponseType::PD_PATCH, patch, NoPatchReason::None, discarded };
    }

    // Patch body with no header — v0.3/v0.4 truncation cuts the top line off.
    // Executing this as Lua is what produced the `unexpected symbol` errors,
    // so report it instead.
    if (content.contains("#X obj") || content.contains("#X connect")
        || content.contains("#X msg"))
        return { ResponseType::NO_PATCH, content, NoPatchReason::PatchFragment };

    // Check every non-empty line for a /pds prefix, not just the first —
    // tolerates a stray label line (e.g. "pd-script") some models prepend
    // despite the system prompt asking them not to. Bridge::execute filters
    // the non-command lines back out before running them.
    for (auto const& line : juce::StringArray::fromLines(content)) {
        if (line.trim().startsWithIgnoreCase("/pds"))
            return { ResponseType::PDS_COMMANDS, content, NoPatchReason::None };
    }

    // Lua needs positive evidence — an explicit ```lua fence or Lua-shaped
    // code — *and* has to actually parse. Without the first test the old
    // fallback fed prose to the interpreter; without the second, a response in
    // some other language that happens to match the shape still would.
    if (tag == "lua" || looksLikeLua(content)) {
        if (isValidLua(content))
            return { ResponseType::LUA_BLOCK, content, NoPatchReason::None };
        return { ResponseType::NO_PATCH, content, NoPatchReason::InvalidLua };
    }

    return { ResponseType::NO_PATCH, content, NoPatchReason::Prose };
}

juce::String PdParser::describe(NoPatchReason reason)
{
    switch (reason) {
        case NoPatchReason::Prose:
            return juce::String::fromUTF8("no patch in response \xe2\x80\x94 the model answered in prose");
        case NoPatchReason::PatchFragment:
            return juce::String::fromUTF8("patch fragment without a '#N canvas' header \xe2\x80\x94 generation was likely truncated");
        case NoPatchReason::InvalidLua:
            return juce::String::fromUTF8("response looked like code but isn't valid Lua \xe2\x80\x94 possibly another language");
        case NoPatchReason::Empty:
            return "empty response";
        case NoPatchReason::None:
        default:
            return {};
    }
}

} // namespace RepentePd
