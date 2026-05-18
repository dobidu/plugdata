/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "CanvasSerializer.h"
#include "Canvas.h"

namespace RepentePd {

juce::String CanvasSerializer::serialize(Canvas* canvas)
{
    if (!canvas || canvas->objects.empty()) return {};
    // getCanvasContent() uses libpd's binbuf to emit the full pd-file, including
    // sub-patches, graphs, and arrays — correct recursion with zero extra code.
    return canvas->patch.getCanvasContent();
}

} // namespace RepentePd
