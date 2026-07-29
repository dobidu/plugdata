/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "Bridge.h"
#include "RepentePd/Bridge/CanvasSerializer.h"
#include "RepentePd/Bridge/PatchMerger.h"
#include "RepentePd/Bridge/PromptNormalizer.h"
#include "RepentePd/Bridge/VerboseLog.h"
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

juce::String Bridge::logLabel() const
{
    auto const& model = client.getConfig().model;
    return (model.isNotEmpty() ? model : juce::String("repente")) + ": ";
}

bool Bridge::send(juce::String const& prompt, bool analyzeOnly,
                  std::function<void(bool)> onDone, juce::String const& audioContext)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::String context;
    if (auto* canvas = editor ? editor->getCurrentCanvas() : nullptr)
        context = CanvasSerializer::serialize(canvas);
    // History replays patch text from earlier turns, which the canvas may have
    // moved on from since. Nothing in the transcript marks which version is
    // live, so say so explicitly here rather than letting the model guess.
    // ASCII only: the build sets no /utf-8, so MSVC reads narrow literals in the
    // system codepage and a non-ASCII character here reaches the model as
    // mojibake. Non-ASCII console text in this file goes through
    // String::fromUTF8 with hex escapes for the same reason.
    if (context.isNotEmpty())
        context = "Current canvas (authoritative; supersedes any patch text "
                  "earlier in this conversation):\n" + context;
    if (audioContext.isNotEmpty())
        context += (context.isNotEmpty() ? "\n" : "") + audioContext;

    if (editor && editor->pd)
        editor->pd->logRepente(logLabel() + (analyzeOnly ? "analyzing..." : "thinking..."));

    // Build full messages array: system (canvas + audio context) + history + new user prompt
    std::vector<RepenteClient::Message> messages;
    if (context.isNotEmpty())
        messages.push_back({"system", context});

    // Rewrite the prompt into the phrasing the model was trained on ("Write a
    // Pure Data patch for ...") instead of prefixing a meta-statement about
    // Pure Data — see PromptNormalizer.h. Toggled by /config rewrite.
    bool const rewrite = SettingsFile::getInstance()->getProperty<bool>("repente_rewrite");
    juce::String const wrappedPrompt = rewrite ? PromptNormalizer::normalize(prompt) : prompt;

    // The new turn only — past turns are already visible in the literal request
    // body that onRawRequest appends below, so don't duplicate them here.
    juce::String newTurnDump;
    for (auto const& m : messages)
        newTurnDump += "[" + m.role + "]\n" + m.content + "\n\n";
    newTurnDump += "[user]\n" + wrappedPrompt;

    // Replay past turns so the model can follow a multi-step conversation.
    // Only turns PdParser recognized are in here — the NO_PATCH filter in the
    // response callback below keeps unparseable replies out, since feeding the
    // model its own malformed output back as an "assistant" example reinforces
    // the same bad pattern rather than letting it self-correct.
    for (auto const& m : conversationHistory)
        messages.push_back({m.role, m.content});

    messages.push_back({"user", wrappedPrompt});

    bool const verbose = SettingsFile::getInstance()->getProperty<bool>("repente_verbose");
    int verboseTurnId = -1;
    // /analyze responses are display-only by design (no PdParser routing), so
    // they always get the crop + popup treatment regardless of the verbose
    // toggle - otherwise there'd be no way to read a long analysis at all.
    if ((verbose || analyzeOnly) && editor && editor->pd) {
        verboseTurnId = VerboseLog::record(newTurnDump);
        // Trailing "[#N]" (not a leading "verbose #N -") so the prompt itself
        // reads naturally; Console.h's right-click handler parses it back out.
        editor->pd->logRepente(logLabel() + prompt + " [#" + juce::String(verboseTurnId) + "]");
    }

    std::function<void(juce::String)> onRawRequest;
    if (verboseTurnId >= 0) {
        onRawRequest = [id = verboseTurnId](juce::String const& rawRequest) {
            VerboseLog::appendRequestDetail(id, rawRequest);
        };
    }

    // Measured around client.send() (the actual network round trip), not the
    // message-building above, so this reflects what a model/host is actually
    // costing you — the number users want when comparing providers.
    double const requestStartMs = juce::Time::getMillisecondCounterHiRes();

    return client.send(messages, [this, analyzeOnly, prompt, verboseTurnId, onDone, requestStartMs](juce::String const& response) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        double const elapsedMs = juce::Time::getMillisecondCounterHiRes() - requestStartMs;
        if (editor && editor->pd)
            editor->pd->logRepente(logLabel() + juce::String(elapsedMs / 1000.0, 2) + "s");

        if (verboseTurnId >= 0)
            VerboseLog::setResponse(verboseTurnId, response, elapsedMs);

        if (response.startsWith("error:")) {
            if (editor && editor->pd)
                editor->pd->logError(response);
            if (onDone) onDone(false);
            return;
        }

        if (analyzeOnly) {
            if (onDone) onDone(true);
            return;
        }

        auto parsed = PdParser::parse(response);

        // Don't save NO_PATCH turns to history — replaying the model's own
        // unrecognized/malformed output back to it as a past "assistant"
        // example tends to reinforce the same bad pattern rather than let it
        // self-correct on the next try.
        if (parsed.type != ResponseType::NO_PATCH) {
            // Store what actually ran, not the raw reply — parsed.content has
            // fences and any discarded preamble stripped, so the model sees its
            // own output replayed in the same form the executor saw it.
            conversationHistory.push_back({"user",      prompt});
            conversationHistory.push_back({"assistant", parsed.content});
            while (static_cast<int>(conversationHistory.size()) > MAX_HISTORY_TURNS * 2)
                conversationHistory.erase(conversationHistory.begin());
            saveHistory();
        }

        execute(parsed);
        if (onDone) onDone(true);
    }, onRawRequest);
}

void Bridge::execute(ParsedResponse const& parsed)
{
    if (!editor) return;

    switch (parsed.type)
    {
        case ResponseType::PD_PATCH:
        {
            if (parsed.discardedLines > 0 && editor->pd)
                editor->pd->logRepente(logLabel() + "discarded " + juce::String(parsed.discardedLines)
                    + " line" + (parsed.discardedLines == 1 ? "" : "s")
                    + " of non-patch text around the patch");
            if (mergeMode) {
                if (editor->pd) editor->pd->logRepente(logLabel() + "merging patch...");
                PatchMerger::merge(parsed.content, editor);
            } else {
                if (editor->pd) editor->pd->logRepente(logLabel() + "opening patch...");
                editor->getTabComponent().openPatch(parsed.content);
                editor->refreshObjectsPanel();
            }
            break;
        }
        case ResponseType::LUA_BLOCK:
        {
            if (editor->pd) editor->pd->logRepente(logLabel() + "running Lua...");
            if (auto* pi = editor->getPromptInput())
                pi->executeCommand(editor->pd, "{" + parsed.content + "}");
            break;
        }
        case ResponseType::PDS_COMMANDS:
        {
            if (editor->pd) editor->pd->logRepente(logLabel() + "executing commands...");
            if (auto* pi = editor->getPromptInput()) {
                // Only run lines that are actually /pds commands — skip any
                // stray label/preamble line PdParser tolerated to classify
                // this as PDS_COMMANDS. Executing an arbitrary non-command
                // line here would fall through PromptInput's dispatch to
                // "free text -> LLM bridge" and re-send it as a new prompt.
                for (auto const& line : juce::StringArray::fromLines(parsed.content)) {
                    auto trimmed = line.trim();
                    if (trimmed.startsWithIgnoreCase("/pds"))
                        pi->executeCommand(editor->pd, trimmed);
                }
            }
            break;
        }
        case ResponseType::NO_PATCH:
        {
            // Nothing executable was recognized. Show the model's text rather
            // than handing it to the Lua engine, which only yields a syntax error.
            if (editor->pd) {
                editor->pd->logError(logLabel() + PdParser::describe(parsed.reason));
                editor->pd->logRepente(String::fromUTF8("\xe2\x94\x80\xe2\x94\x80 response \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
                editor->pd->logRepente(parsed.content);
                editor->pd->logRepente(String::fromUTF8("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
                if (parsed.reason == NoPatchReason::PatchFragment)
                    editor->pd->logRepente(logLabel() + "retry, or raise max_tokens with /config");
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
