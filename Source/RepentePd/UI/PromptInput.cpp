/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PromptInput.h"
#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Commands/SugarExpander.h"

namespace RepentePd {

PromptInput::PromptInput(PluginEditor* editor, Executor* ex)
    : CommandInput(editor)
    , executor(ex)
{
}

SmallArray<std::pair<int, String>> PromptInput::executeCommand(pd::Instance* pdInstance, String msg)
{
    msg = msg.trim();
    if (msg.isEmpty())
        return {};

    // Arrow sugar: → or -> → /pds create + auto-connect
    if (SugarExpander::isArrow(msg)) {
        juce::String const preArrowLast = executor->getLastCreatedName();
        auto const expanded            = SugarExpander::expand(msg, preArrowLast);
        auto cmd                       = CommandParser::parse(expanded);
        executor->submit(cmd, [ex = executor, preArrowLast, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (preArrowLast.isNotEmpty()) {
                juce::String const newName = ex->getLastCreatedName();
                if (newName.isNotEmpty() && newName != preArrowLast) {
                    auto connectCmd = CommandParser::parse(
                        "/pds connect " + preArrowLast + " " + newName + " 0 0");
                    ex->submit(connectCmd, [pd](juce::String const& m) {
                        pd->logMessage(m);
                    });
                }
            }
        });
        return {};
    }

    // /pds DSL → Executor
    if (msg.startsWith("/pds")) {
        juce::String const lastName = executor->getLastCreatedName();
        auto const expanded         = SugarExpander::expand(msg, lastName);
        auto cmd                    = CommandParser::parse(expanded);
        executor->submit(cmd, [pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
        });
        return {};
    }

    // /lua <expr> → wrap in {} and delegate to base Lua engine
    if (msg.startsWith("/lua ")) {
        return CommandInput::executeCommand(pdInstance, "{" + msg.substring(5).trim() + "}");
    }

    // Any other /command → strip leading / and delegate to CommandInput
    if (msg.startsWith("/")) {
        return CommandInput::executeCommand(pdInstance, msg.substring(1));
    }

    // {expr}, <id> > <msg>, > (deselect) → delegate to CommandInput
    if (msg.startsWith("{") || msg.contains(" > ") || msg.trimStart().startsWith(">")) {
        return CommandInput::executeCommand(pdInstance, msg);
    }

    // Free text → Repente LLM prompt (bridge wired in Phase 03)
    pdInstance->logMessage("repente: " + msg + " [Phase 03]");
    return {};
}

} // namespace RepentePd
