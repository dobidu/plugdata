/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h" // brings using namespace juce + JUCE GUI modules (plugdata convention)
#include "Executor.h"
#include "ArrangeEngine.h"
#include "Canvas.h" // needed for canvas->patch mutations and canvas->synchronise()
#include "Object.h"
#include "Utility/SettingsFile.h"
#include <unordered_set>

namespace RepentePd {

namespace {

[[maybe_unused]] juce::String commandTypeName(CommandType t)
{
    switch (t)
    {
        case CommandType::PDS_CREATE:  return "PDS_CREATE";
        case CommandType::PDS_CONNECT: return "PDS_CONNECT";
        case CommandType::PDS_DELETE:  return "PDS_DELETE";
        case CommandType::PDS_LIST:    return "PDS_LIST";
        case CommandType::PDS_MOVE:    return "PDS_MOVE";
        case CommandType::PDS_LUA:     return "PDS_LUA";
        case CommandType::PDS_ARRANGE: return "PDS_ARRANGE";
        case CommandType::HELP:        return "HELP";
        case CommandType::CLEAR:       return "CLEAR";
        case CommandType::PASSTHROUGH: return "PASSTHROUGH";
        default:                       return "UNKNOWN";
    }
}

} // anonymous namespace

Executor::Executor(Canvas* c) : canvas(c) {}

void Executor::setCanvas(Canvas* newCanvas)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (canvas == newCanvas) return;

    if (canvas != nullptr)
        canvasStates[canvas] = { registry, nextId, placementCursor }; // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    canvas = newCanvas;

    if (canvas != nullptr) {
        auto it = canvasStates.find(canvas);
        if (it != canvasStates.end()) {
            registry        = it->second.registry;
            nextId          = it->second.nextId;
            placementCursor = it->second.placementCursor;
            fprintf(stderr, "Executor::setCanvas → %p (restored %zu objects)\n",
                    static_cast<void*>(canvas), registry.size());
        } else {
            registry.clear();
            nextId          = 1;
            placementCursor = {50, 50};
            fprintf(stderr, "Executor::setCanvas → %p (new canvas)\n",
                    static_cast<void*>(canvas));
        }
    } else {
        registry.clear();
        nextId          = 1;
        placementCursor = {50, 50};
    }
}

void Executor::submit(CommandResult const& cmd,
                      std::function<void(juce::String)> const& onResult)
{
    juce::MessageManager::callAsync([this, cmd, onResult] {
        execute(cmd, onResult);
    });
}

juce::String Executor::executeSync(CommandResult const& cmd)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    juce::String result;
    execute(cmd, [&result](juce::String const& r) { result = r; });
    return result;
}

juce::Point<int> Executor::nextAutoPosition()
{
    auto pos = placementCursor;
    placementCursor.x += 90;
    if (placementCursor.x > 700) {
        placementCursor.x = 50;
        placementCursor.y += 60;
    }
    return pos;
}

juce::String Executor::getLastCreatedName() const
{
    if (nextId <= 1) return {};
    return "obj_" + juce::String(nextId - 1);
}

void Executor::execute(CommandResult const& cmd,
                       std::function<void(juce::String)> const& onResult)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (canvas == nullptr)
    {
        if (onResult) onResult("error: no active canvas");
        return;
    }

    if (!cmd.error.isEmpty())
    {
        if (onResult) onResult("error: " + cmd.error);
        return;
    }

    fprintf(stderr, "Executor::execute %s%s\n",
            commandTypeName(cmd.type).toRawUTF8(),
            cmd.args.isEmpty() ? "" : (" " + cmd.args.joinIntoString(" ")).toRawUTF8());

    auto ctx = [this]() -> juce::String {
        return " (" + (canvas ? canvas->patch.getTitle() : juce::String("?")) + ")";
    };

    switch (cmd.type)
    {
        case CommandType::PDS_CREATE:
        {
            if (cmd.args.isEmpty())
            {
                if (onResult) onResult("error: create requires object name");
                return;
            }
            juce::String objText = cmd.args[0];
            bool const hasExplicitCoords = (cmd.args.size() > 2);
            bool const autoplace = SettingsFile::getInstance()->getProperty<bool>("repente_autoplace");
            int x, y;
            if (hasExplicitCoords) {
                x = cmd.args[1].getIntValue();
                y = cmd.args[2].getIntValue();
            } else if (autoplace) {
                auto pos = nextAutoPosition();
                x = pos.x;
                y = pos.y;
            } else {
                x = 100;
                y = 100;
            }

            t_gobj* obj = canvas->patch.createObject(x, y, objText);
            if (obj == nullptr)
            {
                if (onResult) onResult("error: failed to create " + objText);
                return;
            }
            juce::String name = assignName(obj, objText);
            canvas->synchronise();

            // Snap to grid via ObjectGrid after synchronise (Object* now exists)
            Object* lastObj = nullptr;
            for (auto* o : canvas->objects) lastObj = o;
            if (lastObj)
                canvas->objectGrid.positionNewObject(lastObj, juce::Point<int>(x, y));

            if (onResult) onResult("created " + name + ctx());
            break;
        }

        case CommandType::PDS_CONNECT:
        {
            if (cmd.args.size() < 2)
            {
                if (onResult) onResult("error: connect requires src and sink names");
                return;
            }
            void* srcPtr  = resolve(cmd.args[0]);
            void* sinkPtr = resolve(cmd.args[1]);
            if (srcPtr == nullptr)
            {
                if (onResult) onResult("error: unknown object: " + cmd.args[0]);
                return;
            }
            if (sinkPtr == nullptr)
            {
                if (onResult) onResult("error: unknown object: " + cmd.args[1]);
                return;
            }
            int nout = cmd.args.size() > 2 ? cmd.args[2].getIntValue() : 0;
            int nin  = cmd.args.size() > 3 ? cmd.args[3].getIntValue() : 0;

            auto* src  = reinterpret_cast<t_object*>(srcPtr);
            auto* sink = reinterpret_cast<t_object*>(sinkPtr);
            canvas->patch.createConnection(src, nout, sink, nin);
            canvas->synchronise();

            juce::String outStr = cmd.args.size() > 2 ? cmd.args[2] : "0";
            juce::String inStr  = cmd.args.size() > 3 ? cmd.args[3] : "0";
            if (onResult) onResult("connected " + cmd.args[0] + ":" + outStr
                + juce::String::fromUTF8(" \xe2\x86\x92 ") + cmd.args[1] + ":" + inStr + ctx());
            break;
        }

        case CommandType::PDS_DELETE:
        {
            if (cmd.args.isEmpty())
            {
                if (onResult) onResult("error: delete requires object name");
                return;
            }
            void* ptr = resolve(cmd.args[0]);
            if (ptr == nullptr)
            {
                if (onResult) onResult("error: unknown object: " + cmd.args[0]);
                return;
            }
            canvas->patch.removeObjects({ reinterpret_cast<t_gobj*>(ptr) });
            registry.erase(cmd.args[0]); // NOLINT
            canvas->synchronise();
            if (onResult) onResult("deleted " + cmd.args[0] + ctx());
            break;
        }

        case CommandType::PDS_MOVE:
        {
            if (cmd.args.size() < 3)
            {
                if (onResult) onResult("error: move requires name x y");
                return;
            }
            void* ptr = resolve(cmd.args[0]);
            if (ptr == nullptr)
            {
                if (onResult) onResult("error: unknown object: " + cmd.args[0]);
                return;
            }
            int targetX = cmd.args[1].getIntValue();
            int targetY = cmd.args[2].getIntValue();

            // moveObjectTo() uses a different coordinate system (JUCE screen coords + magic offset).
            // Use getObjectBounds + moveObjects delta instead to stay in pd coordinate space.
            int curX = 0, curY = 0, curW = 0, curH = 0;
            auto* cnvPtr = canvas->patch.getRawPointer();
            pd::Interface::getObjectBounds(cnvPtr, reinterpret_cast<t_gobj*>(ptr), &curX, &curY, &curW, &curH);
            canvas->patch.moveObjects({ reinterpret_cast<t_gobj*>(ptr) }, targetX - curX, targetY - curY);
            canvas->synchronise();
            if (onResult) onResult("moved " + cmd.args[0] + " to (" + juce::String(targetX) + ", " + juce::String(targetY) + ")" + ctx());
            break;
        }

        case CommandType::PDS_LIST:
        {
            if (registry.empty())
            {
                if (onResult) onResult("(no objects)");
                return;
            }
            juce::StringArray lines;
            for (auto const& [name, entry] : registry)
                lines.add("  " + name + "  [" + entry.text + "]");
            if (onResult) onResult(lines.joinIntoString("\n"));
            break;
        }

        case CommandType::PDS_ARRANGE:
        {
            auto dir  = cmd.args.isEmpty()
                          ? ArrangeEngine::Direction::LeftRight
                          : ArrangeEngine::parseDirection(cmd.args[0]);
            int  step = cmd.args.size() >= 2 ? cmd.args[1].getIntValue() : 130;
            juce::String result = ArrangeEngine::arrange(canvas, dir, step);
            if (onResult) onResult(result);
            break;
        }

        case CommandType::PDS_LUA:
            if (onResult) onResult("lua: not implemented (Phase 03+)");
            break;

        case CommandType::PASSTHROUGH:
            if (onResult) onResult("passthrough: (Repente bridge in Phase 03)");
            break;

        case CommandType::HELP:
        case CommandType::CLEAR:
            if (onResult) onResult(commandTypeName(cmd.type).toLowerCase() + ": not implemented (Phase 02-03)");
            break;

        default:
            if (onResult) onResult("error: unhandled command type");
            break;
    }
}

juce::String Executor::assignName(void* ptr, juce::String const& text)
{
    juce::String name = "obj_" + juce::String(nextId++);
    registry[name] = { ptr, text };
    return name;
}

void* Executor::resolve(juce::String const& name) const
{
    auto it = registry.find(name);
    return it != registry.end() ? it->second.ptr : nullptr;
}

void Executor::pruneDeletedObjects()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (canvas == nullptr) return;

    std::unordered_set<void*> live;
    for (auto* obj : canvas->objects)
        live.insert(obj->getPointer());

    for (auto it = registry.begin(); it != registry.end(); )
        it = live.count(it->second.ptr) ? std::next(it) : registry.erase(it);
}

void Executor::removeCanvas(Canvas* c)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    canvasStates.erase(c);
    if (canvas == c) {
        canvas  = nullptr;
        registry.clear();
        nextId  = 1;
    }
}

} // namespace RepentePd
