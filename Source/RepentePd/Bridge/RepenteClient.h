/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include "ILlmProvider.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace RepentePd {

// Async HTTP client. Owns one ILlmProvider that controls auth/body/parse shape.
// send() is non-blocking: spawns a detached thread, fires callback on message thread.
class RepenteClient {
public:
    enum class Provider { OpenAI, Anthropic };

    struct Config {
        juce::String url;
        juce::String model;
        juce::String apiKey;
        Provider     provider   = Provider::OpenAI;
        int          timeoutSec = 30;
        int          maxTokens  = 4096;

        Config() : url("http://localhost:11434"), model("llama3.2") {}
    };

    // Backwards-compat type alias for existing callers (Bridge.cpp).
    using Message = LlmMessage;

    explicit RepenteClient(Config cfg = {});
    ~RepenteClient() { cancelled->store(true); }

    bool send(std::vector<Message> const& messages,
              std::function<void(juce::String)> callback);

    void ping(std::function<void(bool, juce::String)> callback);

    void setConfig(Config cfg);
    [[nodiscard]] Config const& getConfig() const { return config; }
    [[nodiscard]] bool isBusy() const { return busy.load(); }

    // Factory: build provider impl for given enum.
    static std::shared_ptr<ILlmProvider> makeProvider(Provider p);

    // Convert string ↔ enum. Used by config commands and SettingsFile.
    static juce::String  providerToString(Provider p);
    static Provider      providerFromString(juce::String const& s);

private:
    Config config;
    std::shared_ptr<ILlmProvider> provider;
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RepenteClient)
};

} // namespace RepentePd
