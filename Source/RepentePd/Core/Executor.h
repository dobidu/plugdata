/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_events/juce_events.h>
#include "RepentePd/Commands/CommandParser.h"
#include <map>

class Canvas;

namespace RepentePd {

// Thread-safe dispatcher: receives CommandResult from any thread, executes
// canvas mutations exclusively on the JUCE message thread via callAsync.
class Executor {
public:
    explicit Executor(Canvas* canvas);
    ~Executor() = default;

    // Safe to call from any thread. onResult fires on message thread.
    void submit(CommandResult const& cmd,
                std::function<void(juce::String)> const& onResult = {});

    // Call from message thread when the active canvas/patch changes.
    void setCanvas(Canvas* newCanvas);

    // Returns auto-assigned name of last created object, or "" if none.
    [[nodiscard]] juce::String getLastCreatedName() const;

    // Returns current canvas (message thread only).
    [[nodiscard]] Canvas* getCanvas() const { return canvas; }

private:
    // Runs exclusively on message thread.
    void execute(CommandResult const& cmd,
                 std::function<void(juce::String)> const& onResult);

    Canvas* canvas = nullptr; // access only on message thread

    // Per-canvas state: registry + nextId saved/restored on tab switches.
    struct CanvasState {
        std::map<juce::String, void*> registry;
        int nextId = 1;
    };
    std::map<Canvas*, CanvasState> canvasStates;
    std::map<juce::String, void*> registry;
    int nextId = 1;

    juce::String  assignName(void* obj);
    [[nodiscard]] void* resolve(juce::String const& name) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Executor)
};

} // namespace RepentePd
