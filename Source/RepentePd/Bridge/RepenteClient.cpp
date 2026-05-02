/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "RepenteClient.h"

// httplib and json included only in .cpp to keep compile times manageable.
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

using json = nlohmann::json;

namespace RepentePd {

RepenteClient::RepenteClient(Config cfg) : config(std::move(cfg)) {}

void RepenteClient::setConfig(Config cfg) { config = std::move(cfg); }

bool RepenteClient::send(juce::String const& prompt,
                         juce::String const& systemContext,
                         std::function<void(juce::String)> callback)
{
    if (busy.exchange(true)) {
        juce::MessageManager::callAsync([cb = std::move(callback)] {
            cb("error: client busy");
        });
        return false;
    }

    auto cfg       = config;
    auto* busy_    = &busy;
    auto  token    = cancelled;

    std::thread([cfg, prompt = prompt, systemContext = systemContext, cb = std::move(callback), busy_, token]() mutable {
        juce::String result;
        try {
            // Parse URL into components
            juce::String url = cfg.url;
            if (url.endsWithChar('/')) url = url.dropLastCharacters(1);

            bool useHttps = url.startsWithIgnoreCase("https://");
            juce::String host = useHttps ? url.substring(8) : url.substring(7);

            juce::String pathPrefix;
            int port = useHttps ? 443 : 80;

            // Extract optional /path prefix before port parsing
            int slashPos = host.indexOf("/");
            if (slashPos >= 0) {
                pathPrefix = host.substring(slashPos);
                host = host.substring(0, slashPos);
            }

            // Extract optional :port
            int colonPos = host.lastIndexOf(":");
            if (colonPos >= 0) {
                port = host.substring(colonPos + 1).getIntValue();
                host = host.substring(0, colonPos);
            }

            std::string endpoint = (pathPrefix + "/v1/chat/completions").toStdString();

            json messages = json::array();
            if (systemContext.isNotEmpty())
                messages.push_back({{"role", "system"}, {"content", systemContext.toStdString()}});
            messages.push_back({{"role", "user"}, {"content", prompt.toStdString()}});

            json body = {
                {"model",    cfg.model.toStdString()},
                {"messages", messages},
                {"stream",   false}
            };
            std::string bodyStr = body.dump();

            httplib::Client cli(host.toStdString(), port);
            cli.set_connection_timeout(cfg.timeoutSec);
            cli.set_read_timeout(cfg.timeoutSec);

            httplib::Headers headers = {
                {"Content-Type", "application/json"},
                {"Accept",       "application/json"}
            };
            if (cfg.apiKey.isNotEmpty())
                headers.emplace("Authorization", "Bearer " + cfg.apiKey.toStdString());

            auto res = cli.Post(endpoint, headers, bodyStr, "application/json");

            if (!res) {
                result = "error: " + juce::String(httplib::to_string(res.error()).c_str());
            } else if (res->status != 200) {
                result = "error: HTTP " + juce::String(res->status);
            } else {
                json resp = json::parse(res->body, nullptr, /*allow_exceptions=*/false);
                if (resp.is_discarded())
                    result = "error: invalid JSON response";
                else
                    result = juce::String(resp["choices"][0]["message"]["content"]
                                             .get<std::string>().c_str());
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
    auto token = cancelled;

    std::thread([cfg, cb = std::move(callback), token]() mutable {
        bool ok = false;
        juce::String msg;
        try {
            juce::String url = cfg.url;
            if (url.endsWithChar('/')) url = url.dropLastCharacters(1);

            bool useHttps = url.startsWithIgnoreCase("https://");
            juce::String host = useHttps ? url.substring(8) : url.substring(7);

            juce::String pathPrefix;
            int port = useHttps ? 443 : 80;

            int slashPos = host.indexOf("/");
            if (slashPos >= 0) {
                pathPrefix = host.substring(slashPos);
                host = host.substring(0, slashPos);
            }

            int colonPos = host.lastIndexOf(":");
            if (colonPos >= 0) {
                port = host.substring(colonPos + 1).getIntValue();
                host = host.substring(0, colonPos);
            }

            std::string endpoint = (pathPrefix + "/v1/models").toStdString();

            httplib::Client cli(host.toStdString(), port);
            cli.set_connection_timeout(5);
            cli.set_read_timeout(5);

            httplib::Headers headers;
            if (cfg.apiKey.isNotEmpty())
                headers.emplace("Authorization", "Bearer " + cfg.apiKey.toStdString());

            auto res = cli.Get(endpoint, headers);

            if (!res) {
                msg = juce::String(httplib::to_string(res.error()).c_str());
            } else if (res->status == 200) {
                ok = true;
                json j = json::parse(res->body, nullptr, false);
                if (!j.is_discarded() && j.contains("data"))
                    msg = "models available: " + juce::String((int)j["data"].size());
                else
                    msg = "connected (status 200)";
            } else {
                msg = "HTTP " + juce::String(res->status);
            }
        }
        catch (std::exception const& e) { msg = juce::String(e.what()); }
        catch (...)                      { msg = "unknown exception"; }

        if (!token->load())
            juce::MessageManager::callAsync([cb, ok, msg] { cb(ok, msg); });
    }).detach();
}

} // namespace RepentePd
