/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "RepenteClient.h"
#include "OpenAIProvider.h"
#include "AnthropicProvider.h"

#include <thread>

namespace RepentePd {

namespace {

// Transport is juce::URL rather than a bundled HTTP library: cloud providers are
// https-only, and juce::URL uses the platform TLS stack (WinINet / NSURLSession /
// libcurl) so no OpenSSL build dependency is needed on any platform.

// juce::URL takes headers as one "Name: value" block separated by CRLF.
juce::String toHeaderBlock(std::vector<std::pair<std::string, std::string>> const& src)
{
    juce::String out;
    for (auto const& p : src)
        out += juce::String(p.first) + ": " + juce::String(p.second) + "\r\n";
    return out;
}

// Blocking; callers run it on a detached thread. Returns the response body and sets
// statusOut (0 when the connection itself failed and no response was received).
juce::String httpRequest(juce::String const& url,
                         juce::String const& headerBlock,
                         juce::String const& postBody,
                         int timeoutSec,
                         int& statusOut)
{
    statusOut = 0;

    auto const isPost = postBody.isNotEmpty();
    juce::URL request(url);
    if (isPost) request = request.withPOSTData(postBody);

    auto options = juce::URL::InputStreamOptions(isPost ? juce::URL::ParameterHandling::inPostData
                                                        : juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headerBlock)
                       .withConnectionTimeoutMs(timeoutSec * 1000)
                       .withStatusCode(&statusOut);

    auto stream = request.createInputStream(options);
    if (stream == nullptr) return {};

    // Read the body even on non-2xx — providers carry their error detail in it.
    return stream->readEntireStreamAsString();
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

juce::String RepenteClient::buildUrl(juce::String const& baseIn, juce::String const& endpointPath)
{
    juce::String base = baseIn.trim();
    while (base.endsWithChar('/')) base = base.dropLastCharacters(1);

    // Base already spells out the whole endpoint.
    if (base.endsWith(endpointPath)) return base;

    // Drop a duplicated leading segment ("…/v1" + "/v1/messages"). Genuine proxy
    // prefixes ("https://host/api" + "/v1/messages") have no overlap and survive.
    int const secondSlash = endpointPath.indexOf(1, "/");
    juce::String const firstSegment = secondSlash > 0 ? endpointPath.substring(0, secondSlash)
                                                      : endpointPath;
    if (base.endsWith(firstSegment))
        base = base.dropLastCharacters(firstSegment.length());

    return base + endpointPath;
}

bool RepenteClient::send(std::vector<Message> const& messages,
                         std::function<void(juce::String)> callback,
                         std::function<void(juce::String)> onRawRequest)
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

    // Built synchronously here (pure string/JSON work, no I/O) rather than
    // inside the detached thread below, so onRawRequest can report the exact
    // request before the network call starts, without any cross-thread hop.
    LlmRequest req;
    req.messages   = messages;
    req.model      = cfg.model;
    req.apiKey     = cfg.apiKey;
    req.maxTokens  = cfg.maxTokens;
    req.timeoutSec = cfg.timeoutSec;

    juce::String const url     = RepenteClient::buildUrl(cfg.url, prov->chatEndpointPath());
    juce::String const bodyStr = juce::String(prov->buildBody(req));

    std::vector<std::pair<std::string, std::string>> hdrs;
    prov->buildHeaders(cfg.apiKey, hdrs);

    if (onRawRequest) {
        juce::String dump = "curl -X POST '" + url + "' \\\n";
        for (auto const& hdr : hdrs) {
            bool const isSecret = hdr.first == "Authorization" || hdr.first == "x-api-key";
            dump += "  -H '" + juce::String(hdr.first) + ": "
                  + (isSecret ? juce::String("***REDACTED***") : juce::String(hdr.second))
                  + "' \\\n";
        }
        dump += "  -d '" + bodyStr + "'";
        onRawRequest(dump);
    }

    juce::String const headerBlock = toHeaderBlock(hdrs);

    std::thread([url, headerBlock, bodyStr, timeoutSec = cfg.timeoutSec, prov, cb = std::move(callback), busy_, token]() mutable {
        juce::String result;
        try {
            int status = 0;
            auto const body = httpRequest(url, headerBlock, bodyStr, timeoutSec, status);

            if (status == 0) {
                result = "error: could not reach " + url + " (connection failed or timed out)";
            } else if (status != 200) {
                // Let provider extract structured error message from body.
                auto parsedErr = prov->parseResponse(body.toStdString());
                result = parsedErr.startsWith("error:")
                             ? parsedErr
                             : "error: HTTP " + juce::String(status);
            } else {
                result = prov->parseResponse(body.toStdString());
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
            juce::String const url = RepenteClient::buildUrl(cfg.url, prov->pingEndpointPath());

            std::vector<std::pair<std::string, std::string>> hdrs;
            prov->buildHeaders(cfg.apiKey, hdrs);

            int status = 0;
            auto const body = httpRequest(url, toHeaderBlock(hdrs), {}, 5, status);

            if (status == 0)
                msg = "could not reach " + url + " (connection failed or timed out)";
            else
                msg = prov->parsePingResponse(body.toStdString(), status, ok);
        }
        catch (std::exception const& e) { msg = juce::String(e.what()); }
        catch (...)                      { msg = "unknown exception"; }

        if (!token->load())
            juce::MessageManager::callAsync([cb, ok, msg] { cb(ok, msg); });
    }).detach();
}

} // namespace RepentePd
