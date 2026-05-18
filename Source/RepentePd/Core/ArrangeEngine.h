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
    enum class Direction { LeftRight, RightLeft, TopDown, BottomUp };

    static Direction parseDirection(juce::String const& s);

    // Arrange all objects on canvas by signal-flow depth.
    // Returns a user-facing status string. Must be called on the message thread.
    static juce::String arrange(Canvas* canvas, Direction dir = Direction::LeftRight, int step = 130);
};

} // namespace RepentePd
