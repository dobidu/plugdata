/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_events/juce_events.h>
#include <functional>
#include <atomic>

namespace RepentePd {

// Async HTTP client for OpenAI-compatible endpoints.
// send() is non-blocking: spawns a detached thread, fires callback on message thread.
class RepenteClient {
public:
    struct Config {
        juce::String url;
        juce::String model;
        juce::String apiKey;
        int          timeoutSec = 30;

        Config() : url("http://localhost:7860"), model("repente-1") {}
    };

    explicit RepenteClient(Config cfg = {});
    ~RepenteClient() { cancelled->store(true); }

    // Fire-and-forget. Callback fires on message thread with response content,
    // or "error: <reason>" on failure. Returns false if client is already busy.
    // systemContext is injected as role:system if non-empty.
    bool send(juce::String const& prompt,
              juce::String const& systemContext,
              std::function<void(juce::String)> callback);

    // Non-blocking GET /models ping. Fires callback(connected, message) on message thread.
    void ping(std::function<void(bool, juce::String)> callback);

    void setConfig(Config cfg);
    [[nodiscard]] Config const& getConfig() const { return config; }
    [[nodiscard]] bool isBusy() const { return busy.load(); }

private:
    Config config;
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RepenteClient)
};

} // namespace RepentePd
