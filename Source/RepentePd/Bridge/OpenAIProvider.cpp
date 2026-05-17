/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "OpenAIProvider.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RepentePd {

void OpenAIProvider::buildHeaders(juce::String const& apiKey,
                                  std::vector<std::pair<std::string, std::string>>& out) const
{
    out.emplace_back("Content-Type", "application/json");
    out.emplace_back("Accept",       "application/json");
    if (apiKey.isNotEmpty())
        out.emplace_back("Authorization", "Bearer " + apiKey.toStdString());
}

std::string OpenAIProvider::buildBody(LlmRequest const& req) const
{
    json messages_json = json::array();
    for (auto const& m : req.messages)
        messages_json.push_back({{"role",    m.role.toStdString()},
                                 {"content", m.content.toStdString()}});

    json body = {
        {"model",    req.model.toStdString()},
        {"messages", messages_json},
        {"stream",   false}
    };
    // max_tokens optional for OpenAI/Ollama; only include if explicitly set (>0).
    if (req.maxTokens > 0)
        body["max_tokens"] = req.maxTokens;
    return body.dump();
}

juce::String OpenAIProvider::parseResponse(std::string const& body) const
{
    json resp = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (resp.is_discarded())
        return "error: invalid JSON response";

    // API-level error
    if (resp.contains("error")) {
        auto const& err = resp["error"];
        std::string msg = err.is_object() && err.contains("message")
                              ? err["message"].get<std::string>()
                              : err.dump();
        return juce::String("error: ") + juce::String(msg.c_str());
    }

    if (!resp.contains("choices") || !resp["choices"].is_array() || resp["choices"].empty())
        return "error: response missing choices";

    auto const& choice0 = resp["choices"][0];
    if (!choice0.contains("message") || !choice0["message"].contains("content"))
        return "error: response missing message.content";

    return juce::String(choice0["message"]["content"].get<std::string>().c_str());
}

juce::String OpenAIProvider::parsePingResponse(std::string const& body, int httpStatus, bool& ok) const
{
    if (httpStatus != 200) {
        ok = false;
        return "HTTP " + juce::String(httpStatus);
    }
    ok = true;
    json j = json::parse(body, nullptr, false);
    if (!j.is_discarded() && j.contains("data") && j["data"].is_array())
        return "models available: " + juce::String((int)j["data"].size());
    return "connected (status 200)";
}

} // namespace RepentePd
