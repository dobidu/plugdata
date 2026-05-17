/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include "ILlmProvider.h"

namespace RepentePd {

// OpenAI Chat Completions shape. Works for OpenAI, Ollama, llama-server, LM Studio,
// OpenRouter, Together, Groq, and any other OpenAI-compatible server.
class OpenAIProvider final : public ILlmProvider {
public:
    [[nodiscard]] juce::String name() const override              { return "openai"; }
    [[nodiscard]] juce::String chatEndpointPath() const override  { return "/v1/chat/completions"; }
    [[nodiscard]] juce::String pingEndpointPath() const override  { return "/v1/models"; }

    void buildHeaders(juce::String const& apiKey,
                      std::vector<std::pair<std::string, std::string>>& out) const override;
    [[nodiscard]] std::string  buildBody(LlmRequest const& req) const override;
    [[nodiscard]] juce::String parseResponse(std::string const& body) const override;
    [[nodiscard]] juce::String parsePingResponse(std::string const& body, int httpStatus, bool& ok) const override;
};

} // namespace RepentePd
