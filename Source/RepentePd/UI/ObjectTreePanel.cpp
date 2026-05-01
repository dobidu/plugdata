/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "ObjectTreePanel.h"
#include "LookAndFeel.h"

namespace RepentePd {

static juce::StringArray const kUINames {
    "bng", "tgl", "hsl", "vsl", "hradio", "vradio", "nbx", "cnv",
    "vu", "floatatom", "symbolatom", "listbox"
};

juce::String ObjectTreePanel::classify(juce::String const& text)
{
    auto base = text.upToFirstOccurrenceOf(" ", false, false).trim();
    if (base.endsWith("~")) return "DSP";
    if (kUINames.contains(base)) return "UI";
    return "Control";
}

ObjectTreePanel::ObjectTreePanel()
{
    displayLines.add("(no objects)");
}

void ObjectTreePanel::refresh(Executor const& executor)
{
    auto const& reg = executor.getRegistry();

    juce::StringArray dsp, ui, control;
    for (auto const& [name, entry] : reg) {
        auto cat = classify(entry.text);
        juce::String line = name + "  [" + entry.text + "]";
        if (cat == "DSP")     dsp.add(line);
        else if (cat == "UI") ui.add(line);
        else                  control.add(line);
    }

    displayLines.clear();
    if (dsp.isEmpty() && ui.isEmpty() && control.isEmpty()) {
        displayLines.add("(no objects)");
        repaint();
        return;
    }

    auto addGroup = [&](juce::String const& header, juce::StringArray const& items) {
        if (items.isEmpty()) return;
        displayLines.add(header);
        for (auto const& n : items)
            displayLines.add("  " + n);
    };
    addGroup("DSP", dsp);
    addGroup("UI", ui);
    addGroup("Control", control);

    repaint();
}

void ObjectTreePanel::paint(juce::Graphics& g)
{
    g.fillAll(PlugDataColours::panelBackgroundColour);

    auto const font       = juce::Font(juce::FontOptions(12.0f));
    auto const headerFont = juce::Font(juce::FontOptions(12.0f, juce::Font::bold));
    int y = 4;
    constexpr int lineH = 18;

    for (auto const& line : displayLines) {
        bool const isHeader = !line.startsWith("  ") && !line.startsWith("(");
        g.setFont(isHeader ? headerFont : font);
        g.setColour(isHeader ? PlugDataColours::toolbarActiveColour
                              : PlugDataColours::toolbarTextColour);
        g.drawText(line, 8, y, getWidth() - 16, lineH,
                   juce::Justification::centredLeft, true);
        y += lineH;
    }
}

} // namespace RepentePd
