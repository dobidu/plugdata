/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include "ILlmProvider.h"

namespace RepentePd {

// Anthropic Messages API (/v1/messages).
// - Auth: x-api-key + anthropic-version headers
// - System messages hoisted out of messages[] into top-level system field
// - Consecutive same-role messages collapsed (Anthropic requires strict alternation)
// - max_tokens REQUIRED
class AnthropicProvider final : public ILlmProvider {
public:
    [[nodiscard]] juce::String name() const override              { return "anthropic"; }
    [[nodiscard]] juce::String chatEndpointPath() const override  { return "/v1/messages"; }
    [[nodiscard]] juce::String pingEndpointPath() const override  { return "/v1/models"; }

    void buildHeaders(juce::String const& apiKey,
                      std::vector<std::pair<std::string, std::string>>& out) const override;
    [[nodiscard]] std::string  buildBody(LlmRequest const& req) const override;
    [[nodiscard]] juce::String parseResponse(std::string const& body) const override;
    [[nodiscard]] juce::String parsePingResponse(std::string const& body, int httpStatus, bool& ok) const override;

    static constexpr char const* kApiVersion = "2023-06-01";
};

} // namespace RepentePd
