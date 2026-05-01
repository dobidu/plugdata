/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PromptInput.h"
#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Commands/SugarExpander.h"

namespace RepentePd {

PromptInput::PromptInput(PluginEditor* ed, Executor* ex)
    : CommandInput(ed)
    , pluginEditor(ed)
    , executor(ex)
{
}

SmallArray<std::pair<int, String>> PromptInput::executeCommand(pd::Instance* pdInstance, String msg)
{
    msg = msg.trim();
    if (msg.isEmpty())
        return {};

    // /help and /clear handled before routing so they always work
    if (msg == "/help") {
        pdInstance->logMessage(
            "/pds create <obj> [x y]     add object to canvas\n"
            "/pds connect <a> <b> [n n]  connect outlets\n"
            "/pds delete <name>          remove object\n"
            "/pds move <name> <x> <y>   reposition object\n"
            "/pds list                   show all objects\n"
            "-> <obj> [x y]             create + connect from last\n"
            "$last                       expand to last created name\n"
            "/lua <expr>                run Lua expression\n"
            "/help                       this help\n"
            "/clear                      clear console\n"
            "<free text>                send to Repente LLM (Phase 03)");
        return {};
    }
    if (msg == "/clear") {
        pluginEditor->clearConsole();
        return {};
    }

    // Arrow sugar: → or -> → /pds create + auto-connect
    if (SugarExpander::isArrow(msg)) {
        juce::String const preArrowLast = executor->getLastCreatedName();
        auto const expanded            = SugarExpander::expand(msg, preArrowLast);
        auto cmd                       = CommandParser::parse(expanded);
        executor->submit(cmd, [this, ex = executor, preArrowLast, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (onRegistryChanged) onRegistryChanged();
            if (preArrowLast.isNotEmpty()) {
                juce::String const newName = ex->getLastCreatedName();
                if (newName.isNotEmpty() && newName != preArrowLast) {
                    auto connectCmd = CommandParser::parse(
                        "/pds connect " + preArrowLast + " " + newName + " 0 0");
                    ex->submit(connectCmd, [this, pd](juce::String const& m) {
                        pd->logMessage(m);
                        if (onRegistryChanged) onRegistryChanged();
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
        executor->submit(cmd, [this, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (onRegistryChanged) onRegistryChanged();
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
