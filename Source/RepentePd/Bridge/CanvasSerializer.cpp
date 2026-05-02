/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "CanvasSerializer.h"
#include "Canvas.h"
#include "Connection.h"
#include "Object.h"
#include "Pd/Interface.h"
#include <unordered_map>

namespace RepentePd {

juce::String CanvasSerializer::serialize(Canvas* canvas)
{
    if (!canvas || canvas->objects.empty()) return {};

    juce::String result;
    result += "#N canvas 0 0 800 600 10;\n";

    // Build Object* → index map for connection emission
    std::unordered_map<Object*, int> indexMap;
    for (int i = 0; i < (int)canvas->objects.size(); ++i)
        indexMap[canvas->objects[i]] = i;

    // Emit objects
    for (auto* obj : canvas->objects) {
        auto bounds = obj->getObjectBounds();
        juce::String text;
        if (auto* ptr = obj->getPointer())
            text = pd::Interface::getObjectText((t_object const*)ptr);
        if (text.isEmpty())
            text = obj->getType();
        result += "#X obj " + juce::String(bounds.getX())
                + " " + juce::String(bounds.getY())
                + " " + text + ";\n";
    }

    // Emit connections
    for (auto* conn : canvas->connections) {
        if (!conn->outobj || !conn->inobj) continue;
        auto srcIt = indexMap.find(conn->outobj.get());
        auto dstIt = indexMap.find(conn->inobj.get());
        if (srcIt == indexMap.end() || dstIt == indexMap.end()) continue;
        result += "#X connect " + juce::String(srcIt->second)
                + " " + juce::String(conn->outIdx)
                + " " + juce::String(dstIt->second)
                + " " + juce::String(conn->inIdx) + ";\n";
    }

    return result;
}

} // namespace RepentePd
