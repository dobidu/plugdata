/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "SugarExpander.h"

namespace RepentePd {

// → is UTF-8 E2 86 92 (3 bytes); -> is 2 bytes
static constexpr char const* ARROW_UTF8 = "\xe2\x86\x92";

bool SugarExpander::isArrow(juce::String const& input)
{
    juce::String t = input.trim();
    return t.startsWith(juce::CharPointer_UTF8(ARROW_UTF8)) || t.startsWith("->");
}

juce::String SugarExpander::expand(juce::String const& input,
                                    juce::String const& lastName)
{
    juce::String trimmed = input.trim();

    // Arrow shorthand: "→ rest" or "-> rest" → "/pds create rest"
    // The arrow-connect logic (submit second connect command) is handled in PluginEditor.
    if (trimmed.startsWith(juce::CharPointer_UTF8(ARROW_UTF8)))
        trimmed = "/pds create " + trimmed.substring(3).trim(); // → is 3 UTF-8 bytes
    else if (trimmed.startsWith("->"))
        trimmed = "/pds create " + trimmed.substring(2).trim();

    // $last substitution
    if (lastName.isNotEmpty())
        trimmed = trimmed.replace("$last", lastName);

    return trimmed;
}

} // namespace RepentePd
