/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "RepentePd/Core/Executor.h"
#include <vector>

class Canvas;
class Object;

namespace RepentePd {

class ObjectTreePanel final : public juce::Component
                            , public juce::ChangeListener {
public:
    ObjectTreePanel();
    ~ObjectTreePanel() override;

    // Full canvas scan — shows all objects; executor provides names for REPL-created ones.
    void refresh(Canvas* canvas, Executor const* executor = nullptr);
    // Legacy: reads from executor registry only.
    void refresh(Executor const& executor);

    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    static juce::String classify(juce::String const& text);

    struct ContentComp : public juce::Component {
        ObjectTreePanel& owner;
        explicit ContentComp(ObjectTreePanel& o) : owner(o) {}
        void paint(juce::Graphics& g) override;
        void mouseDown(juce::MouseEvent const& e) override;
    };

    void updateContentSize();

    ContentComp content_ { *this };
    juce::Viewport vp_;

    juce::StringArray displayLines;
    std::vector<Object*> rowObjects; // parallel to displayLines; nullptr = header/empty
    Canvas* currentCanvas = nullptr;
    int selectedRowIndex  = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectTreePanel)
};

} // namespace RepentePd
