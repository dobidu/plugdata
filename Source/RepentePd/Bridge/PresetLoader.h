/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace RepentePd {

struct Preset {
    juce::String name;
    juce::String url;
    juce::String provider;      // "openai" | "anthropic"
    juce::String model;
    juce::String description;
    juce::String keyEnv;        // env var name (e.g. "ANTHROPIC_API_KEY"); empty if local
    int          maxTokens   = 4096;
    bool         requiresKey = false;
};

class PresetLoader {
public:
    // Returns merged list: bundled defaults overridden by user file (if present).
    // Malformed user JSON is logged + skipped; bundled defaults always returned.
    [[nodiscard]] static std::vector<Preset> loadAll();

    // Find preset by name. Returns nullptr if not found.
    [[nodiscard]] static Preset findOrEmpty(juce::String const& name, bool& found);

    // Path to user override file: <appDataDir>/repente_presets.json
    [[nodiscard]] static juce::File userFile();

    // Write bundled defaults to user file (overwrites). For `/config preset export`.
    static bool writeDefaultsToUser(juce::String& errorOut);

    // Bundled defaults as JSON string. Exposed for tests.
    [[nodiscard]] static juce::String bundledDefaultsJson();

private:
    static std::vector<Preset> parseJson(juce::String const& jsonStr, juce::String& errorOut);
};

} // namespace RepentePd
