/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PatchMerger.h"
#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Core/Executor.h"
#include "PluginEditor.h"
#include <unordered_map>

namespace RepentePd {

void PatchMerger::merge(juce::String const& patchContent, PluginEditor* editor)
{
    if (!editor) return;
    auto* ex = editor->getExecutor();
    auto* pd = editor->pd;
    if (!ex) return;

    std::unordered_map<int, juce::String> indexToName;
    int objIndex = 0;

    // Pass 1: create objects, build index → REPL name map
    for (auto const& rawLine : juce::StringArray::fromLines(patchContent)) {
        auto line = rawLine.trim().trimCharactersAtEnd(";");
        auto tokens = juce::StringArray::fromTokens(line, " ", "");

        if (tokens.size() >= 5 && tokens[0] == "#X" && tokens[1] == "obj") {
            int const x = tokens[2].getIntValue();
            int const y = tokens[3].getIntValue();
            juce::String type = tokens[4];
            juce::String args;
            for (int i = 5; i < tokens.size(); ++i)
                args += (i > 5 ? " " : "") + tokens[i];

            juce::String const cmd = "/pds create " + type
                + (args.isNotEmpty() ? " " + args : "")
                + " " + juce::String(x) + " " + juce::String(y);

            juce::String const prevName = ex->getLastCreatedName();
            auto const result = ex->executeSync(CommandParser::parse(cmd));
            if (pd) pd->logMessage(result);

            juce::String const newName = ex->getLastCreatedName();
            if (newName.isNotEmpty() && newName != prevName)
                indexToName[objIndex] = newName;
        } else if (tokens.size() >= 2 && tokens[0] == "#X"
                && (tokens[1] == "msg" || tokens[1] == "floatatom"
                    || tokens[1] == "symbolatom" || tokens[1] == "text")) {
            // non-obj atoms count as indices but are not created
        } else {
            continue; // #N canvas, #X connect, etc. — no index increment
        }
        ++objIndex;
    }

    // Pass 2: connections
    for (auto const& rawLine : juce::StringArray::fromLines(patchContent)) {
        auto line = rawLine.trim().trimCharactersAtEnd(";");
        auto tokens = juce::StringArray::fromTokens(line, " ", "");

        if (tokens.size() < 6 || tokens[0] != "#X" || tokens[1] != "connect")
            continue;

        int const srcIdx = tokens[2].getIntValue();
        int const srcOut = tokens[3].getIntValue();
        int const dstIdx = tokens[4].getIntValue();
        int const dstIn  = tokens[5].getIntValue();

        auto const srcIt = indexToName.find(srcIdx);
        auto const dstIt = indexToName.find(dstIdx);
        if (srcIt == indexToName.end() || dstIt == indexToName.end()) continue;
        if (srcIt->second.isEmpty() || dstIt->second.isEmpty()) continue;

        juce::String const cmd = "/pds connect "
            + srcIt->second + " " + dstIt->second
            + " " + juce::String(srcOut) + " " + juce::String(dstIn);

        auto const result = ex->executeSync(CommandParser::parse(cmd));
        if (pd) pd->logMessage(result);
    }

    editor->refreshObjectsPanel();
}

} // namespace RepentePd
