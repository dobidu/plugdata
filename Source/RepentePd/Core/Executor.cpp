/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h" // brings using namespace juce + JUCE GUI modules (plugdata convention)
#include "Executor.h"
#include "Canvas.h" // needed for canvas->patch mutations and canvas->synchronise()

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
        canvasStates[canvas] = { registry, nextId };

    canvas = newCanvas;

    if (canvas != nullptr) {
        auto it = canvasStates.find(canvas);
        if (it != canvasStates.end()) {
            registry = it->second.registry;
            nextId   = it->second.nextId;
            fprintf(stderr, "Executor::setCanvas → %p (restored %zu objects)\n",
                    static_cast<void*>(canvas), registry.size());
        } else {
            registry.clear();
            nextId = 1;
            fprintf(stderr, "Executor::setCanvas → %p (new canvas)\n",
                    static_cast<void*>(canvas));
        }
    } else {
        registry.clear();
        nextId = 1;
    }
}

void Executor::submit(CommandResult const& cmd,
                      std::function<void(juce::String)> const& onResult)
{
    juce::MessageManager::callAsync([this, cmd, onResult] {
        execute(cmd, onResult);
    });
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
            int x = cmd.args.size() > 1 ? cmd.args[1].getIntValue() : 100;
            int y = cmd.args.size() > 2 ? cmd.args[2].getIntValue() : 100;

            t_gobj* obj = canvas->patch.createObject(x, y, objText);
            if (obj == nullptr)
            {
                if (onResult) onResult("error: failed to create " + objText);
                return;
            }
            juce::String name = assignName(obj);
            canvas->synchronise();
            if (onResult) onResult("created " + name);
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
            if (onResult) onResult("connected " + cmd.args[0] + ":" + outStr + " → " + cmd.args[1] + ":" + inStr);
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
            registry.erase(cmd.args[0]);
            canvas->synchronise();
            if (onResult) onResult("deleted " + cmd.args[0]);
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
            if (onResult) onResult("moved " + cmd.args[0] + " to (" + juce::String(targetX) + ", " + juce::String(targetY) + ")");
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
            for (auto const& [name, _] : registry)
                lines.add("  " + name);
            if (onResult) onResult(lines.joinIntoString("\n"));
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

juce::String Executor::assignName(void* obj)
{
    juce::String name = "obj_" + juce::String(nextId++);
    registry[name] = obj;
    return name;
}

void* Executor::resolve(juce::String const& name) const
{
    auto it = registry.find(name);
    return it != registry.end() ? it->second : nullptr; // NOLINT
}

} // namespace RepentePd
