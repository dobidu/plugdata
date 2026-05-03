/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "ArrangeEngine.h"
#include "Canvas.h"
#include "Object.h"
#include "Connection.h"
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <map>

namespace RepentePd {

juce::String ArrangeEngine::arrange(Canvas* canvas)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (canvas == nullptr)
        return "arrange: no active canvas";

    std::vector<Object*> objs;
    for (auto* o : canvas->objects)
        if (o->getPointer() != nullptr) objs.push_back(o);

    if (objs.size() < 2)
        return "arrange: nothing to arrange";

    // Build set of objects that have at least one incoming connection
    std::unordered_set<Object*> hasIncoming;
    for (auto* conn : canvas->connections) {
        if (conn->inobj != nullptr)
            hasIncoming.insert(conn->inobj.get());
    }

    // BFS from sources (no incoming) to assign depth per object
    std::unordered_map<Object*, int> depth;
    std::queue<Object*> q;
    for (auto* o : objs) {
        if (hasIncoming.find(o) == hasIncoming.end()) {
            depth[o] = 0;
            q.push(o);
        }
    }
    while (!q.empty()) {
        auto* cur = q.front(); q.pop();
        for (auto* conn : canvas->connections) {
            if (conn->outobj.get() != cur) continue;
            auto* dst = conn->inobj.get();
            if (dst == nullptr) continue;
            int d = depth[cur] + 1;
            auto it = depth.find(dst);
            if (it == depth.end() || it->second < d) {
                depth[dst] = d;
                q.push(dst);
            }
        }
    }

    // Unreached objects (isolated, or in cycles) → park in their own column
    int maxDepth = 0;
    for (auto& [o, d] : depth) maxDepth = std::max(maxDepth, d);
    for (auto* o : objs)
        if (depth.find(o) == depth.end())
            depth[o] = maxDepth + 1;

    // Group objects by depth layer
    std::map<int, std::vector<Object*>> layers;
    for (auto* o : objs) layers[depth[o]].push_back(o);

    // Mark canvas dirty so pd tracks these as edits
    if (auto patchPtr = canvas->patch.getPointer())
        canvas_dirty(patchPtr.get(), 1);

    constexpr int X_START = 60, X_STEP = 130;
    constexpr int Y_START = 60, Y_STEP = 70;

    // Use delta-based move (same pattern as PDS_MOVE in Executor):
    // getObjectBounds returns raw pd coords; moveObjects takes a delta.
    // moveObjectTo(x,y) adds +1542 before calling Interface::moveObject —
    // wrong coord space for objects created at raw pd coords.
    auto* cnvPtr = canvas->patch.getRawPointer();
    for (auto& [layer, layerObjs] : layers) {
        int const targetX = X_START + layer * X_STEP;
        for (int i = 0; i < static_cast<int>(layerObjs.size()); ++i) {
            int const targetY = Y_START + i * Y_STEP;
            auto* gobj = layerObjs[i]->getPointer();
            int curX = 0, curY = 0, curW = 0, curH = 0;
            pd::Interface::getObjectBounds(cnvPtr, gobj, &curX, &curY, &curW, &curH);
            canvas->patch.moveObjects({ gobj }, targetX - curX, targetY - curY);
        }
    }
    canvas->synchronise();

    return "arrange: " + juce::String(static_cast<int>(objs.size()))
           + " objects in " + juce::String(static_cast<int>(layers.size())) + " columns";
}

} // namespace RepentePd
