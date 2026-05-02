/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "RepentePd/Core/Executor.h"

class Canvas;

namespace RepentePd {

class ObjectTreePanel final : public juce::Component {
public:
    ObjectTreePanel();
    ~ObjectTreePanel() override = default;

    // Full canvas scan — shows all objects; executor provides names for REPL-created ones.
    void refresh(Canvas* canvas, Executor const* executor = nullptr);
    // Legacy: reads from executor registry only.
    void refresh(Executor const& executor);

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    static juce::String classify(juce::String const& text);

    juce::StringArray displayLines;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectTreePanel)
};

} // namespace RepentePd
