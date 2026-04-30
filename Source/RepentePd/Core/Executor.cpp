/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Executor.h"

// Canvas.h included only when DirectCommands need canvas mutations (Phase 02-02).
// Forward declaration in Executor.h is sufficient for pointer storage and null checks.

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
    canvas = newCanvas;
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

    DBG("Executor::execute " + commandTypeName(cmd.type)
        + (cmd.args.isEmpty() ? "" : " " + cmd.args.joinIntoString(" ")));

    // DirectCommand dispatch implemented in Phase 02-02.
    if (onResult) onResult("not implemented: " + cmd.raw);
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
