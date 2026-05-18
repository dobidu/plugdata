/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "Bridge.h"
#include "RepentePd/Bridge/CanvasSerializer.h"
#include "RepentePd/Bridge/PatchMerger.h"
#include "RepentePd/UI/PromptInput.h"
#include "PluginEditor.h"
#include "Utility/SettingsFile.h"
#include <algorithm>

namespace RepentePd {

Bridge::Bridge(PluginEditor* ed) : editor(ed) { loadHistory(); }

void Bridge::setConfig(RepenteClient::Config cfg) { client.setConfig(std::move(cfg)); }
RepenteClient::Config const& Bridge::getConfig() const { return client.getConfig(); }
bool Bridge::isBusy() const { return client.isBusy(); }
void Bridge::ping(std::function<void(bool, juce::String)> callback) { client.ping(std::move(callback)); }

bool Bridge::send(juce::String const& prompt, bool analyzeOnly,
                  std::function<void(bool)> onDone, juce::String const& audioContext)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::String context;
    if (auto* canvas = editor ? editor->getCurrentCanvas() : nullptr)
        context = CanvasSerializer::serialize(canvas);
    if (audioContext.isNotEmpty())
        context += (context.isNotEmpty() ? "\n" : "") + audioContext;

    if (editor && editor->pd)
        editor->pd->logRepente(analyzeOnly ? "repente: analyzing..." : "repente: thinking...");

    // Build full messages array: system (canvas + audio) + history + new user prompt
    std::vector<RepenteClient::Message> messages;
    if (context.isNotEmpty())
        messages.push_back({"system", context});
    for (auto const& m : conversationHistory)
        messages.push_back({m.role, m.content});
    messages.push_back({"user", prompt});

    return client.send(messages, [this, analyzeOnly, prompt, onDone](juce::String const& response) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        if (response.startsWith("error:")) {
            if (editor && editor->pd)
                editor->pd->logError(response);
            if (onDone) onDone(false);
            return;
        }

        if (analyzeOnly) {
            if (editor && editor->pd) {
                editor->pd->logRepente(String::fromUTF8("\xe2\x94\x80\xe2\x94\x80 analysis \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
                editor->pd->logRepente(response);
                editor->pd->logRepente(String::fromUTF8("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            }
            if (onDone) onDone(true);
            return;
        }

        // Append to history only for non-analyze successful turns
        conversationHistory.push_back({"user",      prompt});
        conversationHistory.push_back({"assistant", response});
        while (static_cast<int>(conversationHistory.size()) > MAX_HISTORY_TURNS * 2)
            conversationHistory.erase(conversationHistory.begin());
        saveHistory();

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
            if (mergeMode) {
                if (editor->pd) editor->pd->logRepente("repente: merging patch...");
                PatchMerger::merge(parsed.content, editor);
            } else {
                if (editor->pd) editor->pd->logRepente("repente: opening patch...");
                editor->getTabComponent().openPatch(parsed.content);
                editor->refreshObjectsPanel();
            }
            break;
        }
        case ResponseType::LUA_BLOCK:
        {
            if (editor->pd) editor->pd->logRepente("repente: running Lua...");
            if (auto* pi = editor->getPromptInput())
                pi->executeCommand(editor->pd, "{" + parsed.content + "}");
            break;
        }
        case ResponseType::PDS_COMMANDS:
        {
            if (editor->pd) editor->pd->logRepente("repente: executing commands...");
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

void Bridge::clearHistory()
{
    conversationHistory.clear();
    SettingsFile::getInstance()->setProperty("repente_history", juce::String(""));
    SettingsFile::getInstance()->saveSettings();
}

int Bridge::historyTurnCount() const
{
    return static_cast<int>(std::count_if(conversationHistory.begin(), conversationHistory.end(),
        [](HistoryMessage const& m) { return m.role == "user"; }));
}

void Bridge::saveHistory() const
{
    if (!SettingsFile::getInstance()->getProperty<bool>("repente_persist_history"))
        return;
    SettingsFile::getInstance()->setProperty("repente_history", serializeHistory(conversationHistory));
    SettingsFile::getInstance()->saveSettings();
}

void Bridge::loadHistory()
{
    if (!SettingsFile::getInstance()->getProperty<bool>("repente_persist_history"))
        return;
    auto str = SettingsFile::getInstance()->getProperty<juce::String>("repente_history");
    if (str.isNotEmpty())
        conversationHistory = deserializeHistory(str);
}

juce::String Bridge::serializeHistory(std::vector<HistoryMessage> const& history)
{
    juce::Array<juce::var> arr;
    for (auto const& m : history) {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("role",    m.role);
        obj->setProperty("content", m.content);
        arr.add(juce::var(obj));
    }
    return juce::JSON::toString(juce::var(arr), /*allOnOneLine=*/true);
}

std::vector<Bridge::HistoryMessage> Bridge::deserializeHistory(juce::String const& jsonStr)
{
    std::vector<HistoryMessage> result;
    auto parsed = juce::JSON::parse(jsonStr);
    if (auto* arr = parsed.getArray()) {
        for (auto const& item : *arr) {
            if (auto* obj = item.getDynamicObject()) {
                HistoryMessage m;
                m.role    = obj->getProperty("role").toString();
                m.content = obj->getProperty("content").toString();
                if (m.role.isNotEmpty())
                    result.push_back(std::move(m));
            }
        }
    }
    return result;
}

} // namespace RepentePd
