/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_core/juce_core.h>

namespace RepentePd {

// Stateless pre-processor: runs before CommandParser::parse().
// Applies syntactic sugar that makes multi-step patch building ergonomic.
class SugarExpander {
public:
    // Expand sugar in `input` given `lastName` (last assignName() result).
    // Returns expanded string ready for CommandParser::parse().
    static juce::String expand(juce::String const& input,
                               juce::String const& lastName);

    // Returns true if input starts with → or -> (arrow shorthand).
    static bool isArrow(juce::String const& input);
};

} // namespace RepentePd
