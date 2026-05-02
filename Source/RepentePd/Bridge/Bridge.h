/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include "RepenteClient.h"
#include "PdParser.h"
#include <functional>
#include <vector>

class PluginEditor;

namespace RepentePd {

// Orchestrates: prompt → RepenteClient → PdParser → execution path.
// All public methods must be called from the message thread.
class Bridge {
public:
    explicit Bridge(PluginEditor* editor);
    ~Bridge() = default;

    // Fire-and-forget. Logs "repente: thinking..." (or "analyzing...") immediately.
    // onDone(success) fires on message thread when complete.
    // analyzeOnly=true: response logged as plain text, PdParser bypassed.
    // Returns false if client is busy.
    bool send(juce::String const& prompt,
              bool analyzeOnly = false,
              std::function<void(bool)> onDone = {});

    void setConfig(RepenteClient::Config cfg);
    [[nodiscard]] RepenteClient::Config const& getConfig() const;
    [[nodiscard]] bool isBusy() const;
    void ping(std::function<void(bool, juce::String)> callback);

    // When true, PD_PATCH responses are merged into the current canvas instead
    // of opening a new tab.
    void setMergeMode(bool merge) { mergeMode = merge; }
    [[nodiscard]] bool getMergeMode() const { return mergeMode; }

    void clearHistory();
    [[nodiscard]] int historyTurnCount() const;

private:
    void execute(ParsedResponse const& parsed);

    struct HistoryMessage { juce::String role; juce::String content; };
    static constexpr int MAX_HISTORY_TURNS = 20;

    void saveHistory() const;
    void loadHistory();
    static juce::String serializeHistory(std::vector<HistoryMessage> const&);
    static std::vector<HistoryMessage> deserializeHistory(juce::String const&);

    PluginEditor* editor;
    RepenteClient client;
    bool mergeMode = false;
    std::vector<HistoryMessage> conversationHistory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Bridge)
};

} // namespace RepentePd
