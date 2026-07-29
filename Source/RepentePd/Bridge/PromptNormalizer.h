/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_core/juce_core.h>

namespace RepentePd {

// Stateless rewriting of user prompts into the phrasing Repente was trained on.
//
// The model's training prompts name the target language inside the sentence
// ("Write a Pure Data patch for a simple FM synthesizer..."), but users type
// "make an FM synth". Prefixing a meta-statement ("This is about puredata:")
// puts the request in a shape that appears nowhere in training, and the model
// answers in SuperCollider or Max instead. So rather than instructing the model
// about Pure Data, we phrase the request the way it already expects.
class PromptNormalizer {
public:
    enum class Intent {
        AlreadyQualified, // prompt already names Pd — passed through untouched
        Convert,
        Analyze,
        Modify,
        Create            // default
    };

    [[nodiscard]] static Intent classify(juce::String const& prompt);

    // Returns the rewritten prompt. Already-qualified, empty and whitespace-only
    // prompts are returned unchanged.
    [[nodiscard]] static juce::String normalize(juce::String const& prompt);
};

} // namespace RepentePd
