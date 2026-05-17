/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "RepenteClient.h"
#include "OpenAIProvider.h"
#include "AnthropicProvider.h"

#include <cpp-httplib/httplib.h>
#include <thread>

namespace RepentePd {

namespace {

struct ParsedUrl {
    std::string host;
    std::string pathPrefix;
    int  port    = 80;
    bool useHttps = false;
};

ParsedUrl parseUrl(juce::String const& urlIn)
{
    ParsedUrl out;
    juce::String url = urlIn;
    if (url.endsWithChar('/')) url = url.dropLastCharacters(1);

    out.useHttps = url.startsWithIgnoreCase("https://");
    juce::String hostPart = out.useHttps ? url.substring(8) : url.substring(7);
    out.port = out.useHttps ? 443 : 80;

    int const slashPos = hostPart.indexOf("/");
    if (slashPos >= 0) {
        out.pathPrefix = hostPart.substring(slashPos).toStdString();
        hostPart = hostPart.substring(0, slashPos);
    }

    int const colonPos = hostPart.lastIndexOf(":");
    if (colonPos >= 0) {
        out.port = hostPart.substring(colonPos + 1).getIntValue();
        hostPart = hostPart.substring(0, colonPos);
    }

    out.host = hostPart.toStdString();
    return out;
}

auto toHttpLibHeaders(std::vector<std::pair<std::string, std::string>> const& src)
{
    httplib::Headers h;
    for (auto const& p : src) h.emplace(p.first, p.second);
    return h;
}

} // namespace

RepenteClient::RepenteClient(Config cfg) : config(std::move(cfg))
{
    provider = makeProvider(config.provider);
}

void RepenteClient::setConfig(Config cfg)
{
    bool const providerChanged = (cfg.provider != config.provider) || !provider;
    config = std::move(cfg);
    if (providerChanged)
        provider = makeProvider(config.provider);
}

std::shared_ptr<ILlmProvider> RepenteClient::makeProvider(Provider p)
{
    switch (p) {
        case Provider::Anthropic: return std::make_shared<AnthropicProvider>();
        case Provider::OpenAI:
        default:                  return std::make_shared<OpenAIProvider>();
    }
}

juce::String RepenteClient::providerToString(Provider p)
{
    switch (p) {
        case Provider::Anthropic: return "anthropic";
        case Provider::OpenAI:    return "openai";
    }
    return "openai";
}

RepenteClient::Provider RepenteClient::providerFromString(juce::String const& s)
{
    return s.equalsIgnoreCase("anthropic") ? Provider::Anthropic : Provider::OpenAI;
}

bool RepenteClient::send(std::vector<Message> const& messages,
                         std::function<void(juce::String)> callback)
{
    if (busy.exchange(true)) {
        juce::MessageManager::callAsync([cb = std::move(callback)] {
            cb("error: client busy");
        });
        return false;
    }

    auto cfg    = config;
    auto prov   = provider;          // shared_ptr keeps provider alive while detached thread runs
    auto* busy_ = &busy;
    auto  token = cancelled;

    std::thread([cfg, messages, prov, cb = std::move(callback), busy_, token]() mutable {
        juce::String result;
        try {
            auto const parsed = parseUrl(cfg.url);

            LlmRequest req;
            req.messages   = messages;
            req.model      = cfg.model;
            req.apiKey     = cfg.apiKey;
            req.maxTokens  = cfg.maxTokens;
            req.timeoutSec = cfg.timeoutSec;

            std::string const endpoint = parsed.pathPrefix + prov->chatEndpointPath().toStdString();
            std::string const bodyStr  = prov->buildBody(req);

            std::vector<std::pair<std::string, std::string>> hdrs;
            prov->buildHeaders(cfg.apiKey, hdrs);

            httplib::Client cli(parsed.host, parsed.port);
            cli.set_connection_timeout(cfg.timeoutSec);
            cli.set_read_timeout(cfg.timeoutSec);

            auto res = cli.Post(endpoint, toHttpLibHeaders(hdrs), bodyStr, "application/json");

            if (!res) {
                result = "error: " + juce::String(httplib::to_string(res.error()).c_str());
            } else if (res->status != 200) {
                // Let provider extract structured error message from body.
                auto parsedErr = prov->parseResponse(res->body);
                result = parsedErr.startsWith("error:")
                             ? parsedErr
                             : "error: HTTP " + juce::String(res->status);
            } else {
                result = prov->parseResponse(res->body);
            }
        }
        catch (std::exception const& e) {
            result = "error: " + juce::String(e.what());
        }
        catch (...) {
            result = "error: unknown exception";
        }

        busy_->store(false);
        if (!token->load()) {
            auto resultCopy = result;
            juce::MessageManager::callAsync([cb, resultCopy] { cb(resultCopy); });
        }
    }).detach();

    return true;
}

void RepenteClient::ping(std::function<void(bool, juce::String)> callback)
{
    auto cfg   = config;
    auto prov  = provider;
    auto token = cancelled;

    std::thread([cfg, prov, cb = std::move(callback), token]() mutable {
        bool ok = false;
        juce::String msg;
        try {
            auto const parsed = parseUrl(cfg.url);
            std::string const endpoint = parsed.pathPrefix + prov->pingEndpointPath().toStdString();

            std::vector<std::pair<std::string, std::string>> hdrs;
            prov->buildHeaders(cfg.apiKey, hdrs);

            httplib::Client cli(parsed.host, parsed.port);
            cli.set_connection_timeout(5);
            cli.set_read_timeout(5);

            auto res = cli.Get(endpoint, toHttpLibHeaders(hdrs));
            if (!res) {
                msg = juce::String(httplib::to_string(res.error()).c_str());
            } else {
                msg = prov->parsePingResponse(res->body, res->status, ok);
            }
        }
        catch (std::exception const& e) { msg = juce::String(e.what()); }
        catch (...)                      { msg = "unknown exception"; }

        if (!token->load())
            juce::MessageManager::callAsync([cb, ok, msg] { cb(ok, msg); });
    }).detach();
}

} // namespace RepentePd
