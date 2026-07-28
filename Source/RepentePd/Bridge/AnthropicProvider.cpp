/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "AnthropicProvider.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RepentePd {

void AnthropicProvider::buildHeaders(juce::String const& apiKey,
                                     std::vector<std::pair<std::string, std::string>>& out) const
{
    out.emplace_back("Content-Type",      "application/json");
    out.emplace_back("Accept",            "application/json");
    out.emplace_back("anthropic-version", kApiVersion);
    if (apiKey.isNotEmpty())
        out.emplace_back("x-api-key", apiKey.toStdString());
}

std::string AnthropicProvider::buildBody(LlmRequest const& req) const
{
    // 1. Extract all system messages → top-level system field (joined with blank line).
    // 2. Remaining messages: collapse consecutive same-role by joining content with "\n\n".
    juce::String systemText;
    std::vector<LlmMessage> filtered;
    filtered.reserve(req.messages.size());

    for (auto const& m : req.messages) {
        if (m.role == "system") {
            if (systemText.isNotEmpty()) systemText += "\n\n";
            systemText += m.content;
        } else {
            if (!filtered.empty() && filtered.back().role == m.role)
                filtered.back().content += "\n\n" + m.content;
            else
                filtered.push_back(m);
        }
    }

    // Anthropic requires first message to be user. If somehow first is assistant, prepend a stub.
    if (!filtered.empty() && filtered.front().role == "assistant")
        filtered.insert(filtered.begin(), {"user", "continue"});

    json messages_json = json::array();
    for (auto const& m : filtered)
        messages_json.push_back({{"role",    m.role.toStdString()},
                                 {"content", m.content.toStdString()}});

    json body = {
        {"model",      req.model.toStdString()},
        {"messages",   messages_json},
        {"max_tokens", req.maxTokens > 0 ? req.maxTokens : 4096},
        {"stream",     false}
    };
    if (systemText.isNotEmpty()) {
        // Emit system as an array of text blocks with cache_control on the last
        // block so Anthropic caches the canvas + spectral context across turns.
        // Caching is a prefix match — when the canvas does not change between
        // requests (multi-turn conversation, /analyze followups), reads serve at
        // ~10% of base input price. Silently no-ops below the model's minimum
        // cacheable prefix (~1024–4096 tokens depending on model).
        body["system"] = json::array({
            {
                {"type", "text"},
                {"text", systemText.toStdString()},
                {"cache_control", {{"type", "ephemeral"}}}
            }
        });
    }

    return body.dump();
}

juce::String AnthropicProvider::parseResponse(std::string const& body) const
{
    json resp = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (resp.is_discarded())
        return "error: invalid JSON response";

    // API-level error: {"type":"error","error":{"type":"...","message":"..."}}
    if (resp.contains("error")) {
        auto const& err = resp["error"];
        std::string msg = err.is_object() && err.contains("message")
                              ? err["message"].get<std::string>()
                              : err.dump();
        std::string type = err.is_object() && err.contains("type")
                              ? err["type"].get<std::string>() + ": "
                              : "";
        return juce::String("error: ") + juce::String::fromUTF8((type + msg).c_str());
    }

    if (!resp.contains("content") || !resp["content"].is_array() || resp["content"].empty())
        return "error: response missing content";

    // content is array of blocks; concatenate all text blocks.
    juce::String out;
    for (auto const& block : resp["content"]) {
        if (block.is_object() && block.contains("type") && block["type"] == "text"
            && block.contains("text")) {
            out += juce::String::fromUTF8(block["text"].get<std::string>().c_str());
        }
    }
    if (out.isEmpty())
        return "error: response contained no text blocks";
    return out;
}

juce::String AnthropicProvider::parsePingResponse(std::string const& body, int httpStatus, bool& ok) const
{
    if (httpStatus != 200) {
        ok = false;
        // Try to surface Anthropic error message from body.
        json j = json::parse(body, nullptr, false);
        if (!j.is_discarded() && j.contains("error") && j["error"].is_object()
            && j["error"].contains("message"))
            return "HTTP " + juce::String(httpStatus) + juce::String::fromUTF8(" \xe2\x80\x94 ")
                 + juce::String::fromUTF8(j["error"]["message"].get<std::string>().c_str());
        return "HTTP " + juce::String(httpStatus);
    }
    ok = true;
    json j = json::parse(body, nullptr, false);
    if (!j.is_discarded() && j.contains("data") && j["data"].is_array())
        return "models available: " + juce::String((int)j["data"].size());
    return "connected (status 200)";
}

} // namespace RepentePd
