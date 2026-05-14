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

ArrangeEngine::Direction ArrangeEngine::parseDirection(juce::String const& s)
{
    auto t = s.trim().toLowerCase();
    if (t == "right-left" || t == "rl") return Direction::RightLeft;
    if (t == "top-down"   || t == "td") return Direction::TopDown;
    if (t == "bottom-up"  || t == "bu") return Direction::BottomUp;
    return Direction::LeftRight;
}

juce::String ArrangeEngine::arrange(Canvas* canvas, Direction dir, int step)
{
    step = std::max(40, step);
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

    constexpr int MAIN_START = 60, CROSS_START = 60;
    int const MAIN_STEP  = step;
    int const CROSS_STEP = std::max(40, step * 70 / 130);

    bool const isHorizontal = (dir == Direction::LeftRight || dir == Direction::RightLeft);
    bool const isReversed   = (dir == Direction::RightLeft || dir == Direction::BottomUp);

    // Use delta-based move (same pattern as PDS_MOVE in Executor):
    // getObjectBounds returns raw pd coords; moveObjects takes a delta.
    // moveObjectTo(x,y) adds +1542 before calling Interface::moveObject —
    // wrong coord space for objects created at raw pd coords.
    auto* cnvPtr = canvas->patch.getRawPointer();
    for (auto& [layer, layerObjs] : layers) {
        int const mainPos  = MAIN_START + (isReversed ? (maxDepth - layer) : layer) * MAIN_STEP;
        for (int i = 0; i < static_cast<int>(layerObjs.size()); ++i) {
            int const crossPos = CROSS_START + i * CROSS_STEP;
            int const targetX  = isHorizontal ? mainPos  : crossPos;
            int const targetY  = isHorizontal ? crossPos : mainPos;
            auto* gobj = layerObjs[i]->getPointer();
            int curX = 0, curY = 0, curW = 0, curH = 0;
            pd::Interface::getObjectBounds(cnvPtr, gobj, &curX, &curY, &curW, &curH);
            canvas->patch.moveObjects({ gobj }, targetX - curX, targetY - curY);
        }
    }
    canvas->synchronise();

    juce::String const axis = isHorizontal ? "columns" : "rows";
    return "arrange: " + juce::String(static_cast<int>(objs.size()))
           + " objects in " + juce::String(static_cast<int>(layers.size())) + " " + axis;
}

} // namespace RepentePd
