/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PromptBar : public juce::Component {
public:
    PromptBar();
    ~PromptBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::TextEditor& getInput() { return input; }

private:
    juce::TextEditor input;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PromptBar)
};
