/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>

class Canvas;

namespace RepentePd {

// Serialize the active canvas to pd-file format for LLM context injection.
// Must be called from the message thread.
class CanvasSerializer {
public:
    static juce::String serialize(Canvas* canvas);
};

} // namespace RepentePd
