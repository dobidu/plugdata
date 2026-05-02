/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "Bridge.h"
#include "RepentePd/UI/PromptInput.h"
#include "PluginEditor.h"

namespace RepentePd {

Bridge::Bridge(PluginEditor* ed) : editor(ed) {}

void Bridge::setConfig(RepenteClient::Config cfg) { client.setConfig(std::move(cfg)); }
RepenteClient::Config const& Bridge::getConfig() const { return client.getConfig(); }
bool Bridge::isBusy() const { return client.isBusy(); }

bool Bridge::send(juce::String const& prompt, std::function<void(bool)> onDone)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (editor && editor->pd)
        editor->pd->logMessage("repente: thinking...");

    return client.send(prompt, [this, onDone](juce::String const& response) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        if (response.startsWith("error:")) {
            if (editor && editor->pd)
                editor->pd->logMessage(response);
            if (onDone) onDone(false);
            return;
        }

        auto parsed = PdParser::parse(response);
        execute(parsed);
        if (onDone) onDone(true);
    });
}

void Bridge::execute(ParsedResponse const& parsed)
{
    if (!editor) return;

    switch (parsed.type)
    {
        case ResponseType::PD_PATCH:
        {
            if (editor->pd) editor->pd->logMessage("repente: opening patch...");
            editor->getTabComponent().openPatch(parsed.content);
            break;
        }
        case ResponseType::LUA_BLOCK:
        {
            if (editor->pd) editor->pd->logMessage("repente: running Lua...");
            if (auto* pi = editor->getPromptInput())
                pi->executeCommand(editor->pd, "{" + parsed.content + "}");
            break;
        }
        case ResponseType::PDS_COMMANDS:
        {
            if (editor->pd) editor->pd->logMessage("repente: executing commands...");
            if (auto* pi = editor->getPromptInput()) {
                for (auto const& line : juce::StringArray::fromLines(parsed.content)) {
                    auto trimmed = line.trim();
                    if (trimmed.isNotEmpty())
                        pi->executeCommand(editor->pd, trimmed);
                }
            }
            break;
        }
    }
}

} // namespace RepentePd
