/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "PromptBar.h"

PromptBar::PromptBar()
{
    input.setMultiLine(false);
    input.setReturnKeyStartsNewLine(false);
    input.setScrollbarsShown(false);
    input.setPopupMenuEnabled(false);
    addAndMakeVisible(input);
}

void PromptBar::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId).darker(0.15f));
}

void PromptBar::resized()
{
    input.setBounds(getLocalBounds().reduced(4, 2));
}
