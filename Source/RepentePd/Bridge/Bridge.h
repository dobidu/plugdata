/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include "RepenteClient.h"
#include "PdParser.h"
#include <functional>

class PluginEditor;

namespace RepentePd {

// Orchestrates: prompt → RepenteClient → PdParser → execution path.
// All public methods must be called from the message thread.
class Bridge {
public:
    explicit Bridge(PluginEditor* editor);
    ~Bridge() = default;

    // Fire-and-forget. Logs "repente: thinking..." immediately.
    // onDone(success) fires on message thread when complete.
    // Returns false if client is busy.
    bool send(juce::String const& prompt,
              std::function<void(bool)> onDone = {});

    void setConfig(RepenteClient::Config cfg);
    [[nodiscard]] RepenteClient::Config const& getConfig() const;
    [[nodiscard]] bool isBusy() const;
    void ping(std::function<void(bool, juce::String)> callback);

private:
    void execute(ParsedResponse const& parsed);

    PluginEditor* editor;
    RepenteClient client;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Bridge)
};

} // namespace RepentePd
