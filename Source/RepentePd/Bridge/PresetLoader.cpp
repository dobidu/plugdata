/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PresetLoader.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RepentePd {

juce::String PresetLoader::bundledDefaultsJson()
{
    // Default preset bundle. Edit user file (PresetLoader::userFile()) to override or add.
    return juce::String(R"json(
{
  "version": 1,
  "presets": {
    "claude-opus": {
      "url": "https://api.anthropic.com",
      "provider": "anthropic",
      "model": "claude-opus-4-7",
      "max_tokens": 4096,
      "description": "Claude Opus 4.7 via Anthropic API",
      "key_env": "ANTHROPIC_API_KEY",
      "requires_key": true
    },
    "claude-sonnet": {
      "url": "https://api.anthropic.com",
      "provider": "anthropic",
      "model": "claude-sonnet-4-6",
      "max_tokens": 4096,
      "description": "Claude Sonnet 4.6 via Anthropic API",
      "key_env": "ANTHROPIC_API_KEY",
      "requires_key": true
    },
    "claude-haiku": {
      "url": "https://api.anthropic.com",
      "provider": "anthropic",
      "model": "claude-haiku-4-5-20251001",
      "max_tokens": 4096,
      "description": "Claude Haiku 4.5 via Anthropic API",
      "key_env": "ANTHROPIC_API_KEY",
      "requires_key": true
    },
    "gpt-4o": {
      "url": "https://api.openai.com",
      "provider": "openai",
      "model": "gpt-4o",
      "max_tokens": 4096,
      "description": "OpenAI GPT-4o",
      "key_env": "OPENAI_API_KEY",
      "requires_key": true
    },
    "ollama": {
      "url": "http://localhost:11434",
      "provider": "openai",
      "model": "llama3.2",
      "max_tokens": 4096,
      "description": "Ollama local (OpenAI-compat)",
      "key_env": "",
      "requires_key": false
    },
    "repente": {
      "url": "http://localhost:7860",
      "provider": "openai",
      "model": "repente-1",
      "max_tokens": 4096,
      "description": "Repente fine-tuned local server",
      "key_env": "",
      "requires_key": false
    }
  }
}
)json");
}

juce::File PresetLoader::userFile()
{
    return ProjectInfo::appDataDir.getChildFile("repente_presets.json");
}

std::vector<Preset> PresetLoader::parseJson(juce::String const& jsonStr, juce::String& errorOut)
{
    std::vector<Preset> result;
    json j = json::parse(jsonStr.toStdString(), nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        errorOut = "invalid JSON";
        return result;
    }
    if (!j.contains("presets") || !j["presets"].is_object()) {
        errorOut = "missing 'presets' object";
        return result;
    }

    for (auto it = j["presets"].begin(); it != j["presets"].end(); ++it) {
        auto const& obj = it.value();
        if (!obj.is_object()) continue;

        Preset p;
        p.name = juce::String(it.key().c_str());
        if (obj.contains("url")          && obj["url"].is_string())          p.url         = juce::String(obj["url"].get<std::string>().c_str());
        if (obj.contains("provider")     && obj["provider"].is_string())     p.provider    = juce::String(obj["provider"].get<std::string>().c_str());
        if (obj.contains("model")        && obj["model"].is_string())        p.model       = juce::String(obj["model"].get<std::string>().c_str());
        if (obj.contains("description")  && obj["description"].is_string())  p.description = juce::String(obj["description"].get<std::string>().c_str());
        if (obj.contains("key_env")      && obj["key_env"].is_string())      p.keyEnv      = juce::String(obj["key_env"].get<std::string>().c_str());
        if (obj.contains("max_tokens")   && obj["max_tokens"].is_number())   p.maxTokens   = obj["max_tokens"].get<int>();
        if (obj.contains("requires_key") && obj["requires_key"].is_boolean()) p.requiresKey = obj["requires_key"].get<bool>();

        // Minimum viable preset: name + url + provider + model
        if (p.url.isEmpty() || p.provider.isEmpty() || p.model.isEmpty()) continue;
        result.push_back(std::move(p));
    }
    return result;
}

std::vector<Preset> PresetLoader::loadAll()
{
    juce::String err;
    auto defaults = parseJson(bundledDefaultsJson(), err);
    if (!err.isEmpty())
        DBG("repente: bundled preset defaults failed to parse: " + err);

    auto const uf = userFile();
    if (!uf.existsAsFile()) return defaults;

    juce::String userJson = uf.loadFileAsString();
    juce::String userErr;
    auto userPresets = parseJson(userJson, userErr);
    if (!userErr.isEmpty()) {
        DBG("repente: user preset file ignored: " + userErr);
        return defaults;
    }

    // Merge: user entry replaces default w/ same name; new names appended.
    for (auto const& up : userPresets) {
        bool replaced = false;
        for (auto& dp : defaults) {
            if (dp.name == up.name) { dp = up; replaced = true; break; }
        }
        if (!replaced) defaults.push_back(up);
    }
    return defaults;
}

Preset PresetLoader::findOrEmpty(juce::String const& name, bool& found)
{
    auto all = loadAll();
    for (auto const& p : all) {
        if (p.name.equalsIgnoreCase(name)) { found = true; return p; }
    }
    found = false;
    return {};
}

bool PresetLoader::writeDefaultsToUser(juce::String& errorOut)
{
    auto const uf = userFile();
    auto const parent = uf.getParentDirectory();
    if (!parent.exists() && !parent.createDirectory()) {
        errorOut = "cannot create directory: " + parent.getFullPathName();
        return false;
    }
    if (!uf.replaceWithText(bundledDefaultsJson())) {
        errorOut = "cannot write: " + uf.getFullPathName();
        return false;
    }
    return true;
}

} // namespace RepentePd
