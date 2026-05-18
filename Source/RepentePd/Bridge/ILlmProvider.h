/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>
#include <string>
#include <utility>
#include <vector>

namespace RepentePd {

struct LlmMessage {
    juce::String role;     // "system" | "user" | "assistant"
    juce::String content;
};

struct LlmRequest {
    std::vector<LlmMessage> messages;
    juce::String model;
    juce::String apiKey;
    int          maxTokens  = 4096;
    int          timeoutSec = 30;
};

// Provider-agnostic LLM interface.
// Each provider knows its own endpoint paths, auth scheme, body shape, and response parsing.
// RepenteClient owns one of these and handles transport (URL parse, httplib POST/GET).
class ILlmProvider {
public:
    virtual ~ILlmProvider() = default;

    [[nodiscard]] virtual juce::String name() const = 0;                   // "openai" | "anthropic"
    [[nodiscard]] virtual juce::String chatEndpointPath() const = 0;       // "/v1/chat/completions" | "/v1/messages"
    [[nodiscard]] virtual juce::String pingEndpointPath() const = 0;       // "/v1/models" for both

    virtual void buildHeaders(juce::String const& apiKey,
                              std::vector<std::pair<std::string, std::string>>& out) const = 0;

    [[nodiscard]] virtual std::string buildBody(LlmRequest const& req) const = 0;

    // Returns assistant text on success, "error: <reason>" on parse failure or API error.
    [[nodiscard]] virtual juce::String parseResponse(std::string const& body) const = 0;

    // Parse ping response. Returns human-readable status line; sets ok via outparam.
    virtual juce::String parsePingResponse(std::string const& body, int httpStatus, bool& ok) const = 0;
};

} // namespace RepentePd
