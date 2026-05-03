/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_core/juce_core.h>

class Canvas;

namespace RepentePd {

class ArrangeEngine {
public:
    // Arrange all objects on canvas by signal-flow depth (sources left, sinks right).
    // Returns a user-facing status string. Must be called on the message thread.
    static juce::String arrange(Canvas* canvas);
};

} // namespace RepentePd
