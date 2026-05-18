/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace RepentePd {

// Stateless spectral analysis: FFT → 5 frequency bands + peak frequencies → LLM text.
class SpectralAnalyzer {
public:
    struct Result {
        float sub     = 0.f; // 0–60 Hz, normalized [0,1]
        float low     = 0.f; // 60–250 Hz
        float midLow  = 0.f; // 250–2000 Hz
        float midHigh = 0.f; // 2000–6000 Hz
        float high    = 0.f; // 6000–20000 Hz
        std::vector<float> peakFrequencies; // top 3 peaks in Hz, descending magnitude
        int sampleRate = 44100;
    };

    // Multi-frame FFT analysis. Returns zero Result if buffer is empty or silent.
    static Result analyze(juce::AudioBuffer<float> const& buffer, int sampleRate);

    // Format result as LLM-readable string:
    // "spectral: sub=X.XX low=X.XX mid=X.XX hmid=X.XX high=X.XX | peaks: NNNHz ..."
    static juce::String format(Result const& r);
};

} // namespace RepentePd
