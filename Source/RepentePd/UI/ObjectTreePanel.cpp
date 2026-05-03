/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "ObjectTreePanel.h"
#include "Canvas.h"
#include "Object.h"
#include "CanvasViewport.h"
#include "Pd/Interface.h"
#include "LookAndFeel.h"
#include <unordered_map>

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
    rowObjects.push_back(nullptr);
}

ObjectTreePanel::~ObjectTreePanel()
{
    if (currentCanvas)
        currentCanvas->selectedComponents.removeChangeListener(this);
}

void ObjectTreePanel::refresh(Canvas* canvas, Executor const* executor)
{
    // Unregister from old canvas before switching
    if (currentCanvas && currentCanvas != canvas)
        currentCanvas->selectedComponents.removeChangeListener(this);
    currentCanvas = canvas;
    selectedRowIndex = -1;

    displayLines.clear();
    rowObjects.clear();

    if (!canvas || canvas->objects.empty()) {
        displayLines.add("(no objects)");
        rowObjects.push_back(nullptr);
        repaint();
        return;
    }

    // Build ptr → name lookup from executor registry
    std::unordered_map<void*, juce::String> ptrToName;
    if (executor) {
        for (auto const& [name, entry] : executor->getRegistry())
            if (entry.ptr) ptrToName[entry.ptr] = name;
    }

    juce::StringArray dsp, ui, ctrl;
    std::vector<Object*> dspPtrs, uiPtrs, ctrlPtrs;

    for (auto* obj : canvas->objects) {
        juce::String text;
        if (auto* ptr = obj->getPointer())
            text = pd::Interface::getObjectText((t_object const*)ptr);
        if (text.isEmpty())
            text = obj->getType();
        if (text.isEmpty()) continue;

        auto it = ptrToName.find(obj->getPointer());
        juce::String line = (it != ptrToName.end())
            ? it->second + "  [" + text + "]"
            : "[" + text + "]";

        auto cat = classify(text);
        if (cat == "DSP")     { dsp.add(line);  dspPtrs.push_back(obj); }
        else if (cat == "UI") { ui.add(line);   uiPtrs.push_back(obj); }
        else                  { ctrl.add(line); ctrlPtrs.push_back(obj); }
    }

    if (dsp.isEmpty() && ui.isEmpty() && ctrl.isEmpty()) {
        displayLines.add("(no objects)");
        rowObjects.push_back(nullptr);
        repaint();
        return;
    }

    auto addGroup = [&](juce::String const& header, juce::StringArray const& items,
                        std::vector<Object*> const& ptrs) {
        if (items.isEmpty()) return;
        displayLines.add(header);
        rowObjects.push_back(nullptr); // header = not selectable
        for (int i = 0; i < items.size(); ++i) {
            displayLines.add("  " + items[i]);
            rowObjects.push_back(ptrs[static_cast<size_t>(i)]);
        }
    };

    addGroup("DSP",     dsp,  dspPtrs);
    addGroup("UI",      ui,   uiPtrs);
    addGroup("Control", ctrl, ctrlPtrs);

    // Register for canvas selection changes (bidirectional sync)
    if (canvas)
        canvas->selectedComponents.addChangeListener(this);

    repaint();
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
    rowObjects.clear();

    if (dsp.isEmpty() && ui.isEmpty() && control.isEmpty()) {
        displayLines.add("(no objects)");
        rowObjects.push_back(nullptr);
        repaint();
        return;
    }

    // Legacy path: no Object* available, all rows non-selectable
    auto addGroup = [&](juce::String const& header, juce::StringArray const& items) {
        if (items.isEmpty()) return;
        displayLines.add(header);
        rowObjects.push_back(nullptr);
        for (auto const& n : items) {
            displayLines.add("  " + n);
            rowObjects.push_back(nullptr);
        }
    };
    addGroup("DSP",     dsp);
    addGroup("UI",      ui);
    addGroup("Control", control);

    repaint();
}

void ObjectTreePanel::mouseDown(juce::MouseEvent const& e)
{
    constexpr int lineH = 18;
    int const row = (e.getPosition().y - 4) / lineH;
    if (row < 0 || row >= static_cast<int>(rowObjects.size())) return;

    auto* obj = rowObjects[static_cast<size_t>(row)];
    if (!obj || !currentCanvas) return;

    selectedRowIndex = row;
    repaint();

    currentCanvas->deselectAll();
    currentCanvas->setSelected(obj, true, true, true);

    // Scroll to center object in viewport
    if (currentCanvas->viewport) {
        auto const objCenter = obj->getBounds().getCentre().toFloat();
        auto const viewArea  = currentCanvas->viewport->getViewArea();
        auto const newPos    = objCenter - juce::Point<float>(
            viewArea.getWidth() / 2.0f, viewArea.getHeight() / 2.0f);
        currentCanvas->viewport->setViewPosition(newPos);
    }
}

void ObjectTreePanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (!currentCanvas) return;

    // Find the first selected Object on canvas
    Object* sel = nullptr;
    for (int i = 0; i < currentCanvas->selectedComponents.getNumSelected(); ++i) {
        if (auto* comp = currentCanvas->selectedComponents.getSelectedItem(i).get())
            if (auto* obj = dynamic_cast<Object*>(comp)) { sel = obj; break; }
    }

    // Find its row in our mapping
    int found = -1;
    if (sel) {
        for (int i = 0; i < static_cast<int>(rowObjects.size()); ++i) {
            if (rowObjects[static_cast<size_t>(i)] == sel) { found = i; break; }
        }
    }

    if (found != selectedRowIndex) {
        selectedRowIndex = found;
        repaint();
    }
}

void ObjectTreePanel::paint(juce::Graphics& g)
{
    g.fillAll(PlugDataColours::panelBackgroundColour);

    auto const font       = juce::Font(juce::FontOptions(12.0f));
    auto const headerFont = juce::Font(juce::FontOptions(12.0f, juce::Font::bold));
    int y = 4;
    constexpr int lineH = 18;

    for (int i = 0; i < displayLines.size(); ++i) {
        auto const& line = displayLines[i];
        bool const isHeader = !line.startsWith("  ") && !line.startsWith("(");

        bool const isSelected = (i == selectedRowIndex && !isHeader);

        if (isSelected) {
            g.setColour(PlugDataColours::sidebarActiveBackgroundColour);
            g.fillRect(0, y, getWidth(), lineH);
            g.setColour(PlugDataColours::toolbarActiveColour);
            g.fillRect(0, y, 2, lineH);
        }

        bool const useBold = isHeader || isSelected;
        g.setFont(useBold ? headerFont : font);
        g.setColour(isHeader ? PlugDataColours::toolbarActiveColour
                             : PlugDataColours::toolbarTextColour);
        g.drawText(line, 8, y, getWidth() - 16, lineH,
                   juce::Justification::centredLeft, true);
        y += lineH;
    }
}

} // namespace RepentePd
