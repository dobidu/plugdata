/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "SpectralAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace RepentePd {

static constexpr int kFftOrder = 11;
static constexpr int kFftSize  = 1 << kFftOrder; // 2048
static constexpr int kHopSize  = kFftSize / 2;   // 1024

SpectralAnalyzer::Result SpectralAnalyzer::analyze(juce::AudioBuffer<float> const& buffer,
                                                    int sampleRate)
{
    Result result;
    result.sampleRate = sampleRate;

    int const numSamples  = buffer.getNumSamples();
    int const numChannels = buffer.getNumChannels();

    if (numSamples < kFftSize || numChannels == 0 || sampleRate <= 0)
        return result;

    // Mix down to mono
    std::vector<float> mono(static_cast<size_t>(numSamples), 0.f);
    float const channelScale = 1.f / static_cast<float>(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            mono[static_cast<size_t>(i)] += buffer.getSample(ch, i) * channelScale;

    // Precompute Hann window
    std::vector<float> window(static_cast<size_t>(kFftSize));
    for (int i = 0; i < kFftSize; ++i)
        window[static_cast<size_t>(i)] = 0.5f * (1.f - std::cos(
            2.f * juce::MathConstants<float>::pi
            * static_cast<float>(i) / static_cast<float>(kFftSize - 1)));

    // FFT work buffer: 2 * kFftSize complex (interleaved real/imag)
    std::vector<float> fftData(static_cast<size_t>(kFftSize * 2), 0.f);

    // Averaged magnitude spectrum — only positive frequencies [0, kFftSize/2)
    int const specSize = kFftSize / 2;
    std::vector<float> avgSpectrum(static_cast<size_t>(specSize), 0.f);

    juce::dsp::FFT fft(kFftOrder);
    int frameCount = 0;

    for (int offset = 0; offset + kFftSize <= numSamples; offset += kHopSize) {
        // Fill lower half with windowed samples; upper half stays zero
        std::fill(fftData.begin(), fftData.end(), 0.f);
        for (int i = 0; i < kFftSize; ++i)
            fftData[static_cast<size_t>(i)] =
                mono[static_cast<size_t>(offset) + static_cast<size_t>(i)]
                * window[static_cast<size_t>(i)];

        fft.performRealOnlyForwardTransform(fftData.data());

        // Accumulate magnitude per bin (complex interleaved: re at 2i, im at 2i+1)
        for (int i = 0; i < specSize; ++i)
            avgSpectrum[static_cast<size_t>(i)] +=
                std::hypot(fftData[static_cast<size_t>(i) * 2],
                           fftData[static_cast<size_t>(i) * 2 + 1]);
        ++frameCount;
    }

    if (frameCount == 0)
        return result;

    // Normalize by frame count
    float const frameScale = 1.f / static_cast<float>(frameCount);
    for (auto& v : avgSpectrum) v *= frameScale;

    // Compute dynamic band boundaries (bin = round(hz * kFftSize / SR))
    auto binForHz = [&](float hz) {
        return juce::jmin(static_cast<int>(hz * kFftSize / static_cast<float>(sampleRate)),
                          specSize - 1);
    };
    int const subEnd    = binForHz(60.f);
    int const lowEnd    = binForHz(250.f);
    int const midLowEnd = binForHz(2000.f);
    int const midHiEnd  = binForHz(6000.f);
    int const highEnd   = binForHz(20000.f);

    auto sumBand = [&](int from, int to) {
        float s = 0.f;
        for (int i = from; i <= to && i < specSize; ++i)
            s += avgSpectrum[static_cast<size_t>(i)];
        return s;
    };

    result.sub     = sumBand(0,          subEnd);
    result.low     = sumBand(subEnd + 1, lowEnd);
    result.midLow  = sumBand(lowEnd + 1, midLowEnd);
    result.midHigh = sumBand(midLowEnd + 1, midHiEnd);
    result.high    = sumBand(midHiEnd + 1, highEnd);

    // Normalize bands to [0,1] relative to loudest
    float maxBand = std::max({result.sub, result.low, result.midLow,
                              result.midHigh, result.high});
    if (maxBand > 0.f) {
        float const inv = 1.f / maxBand;
        result.sub     *= inv;
        result.low     *= inv;
        result.midLow  *= inv;
        result.midHigh *= inv;
        result.high    *= inv;
    }

    // Peak detection: collect local maxima (exclude DC bin 0)
    using Peak = std::pair<float, int>; // (magnitude, bin)
    std::vector<Peak> peaks;
    for (int i = 1; i < specSize - 1; ++i) {
        auto const idx  = static_cast<size_t>(i);
        float const mag = avgSpectrum[idx];
        if (mag > avgSpectrum[idx - 1] && mag > avgSpectrum[idx + 1])
            peaks.emplace_back(mag, i);
    }

    std::sort(peaks.begin(), peaks.end(),
              [](Peak const& a, Peak const& b) { return a.first > b.first; });

    int const numPeaks = juce::jmin(static_cast<int>(peaks.size()), 3);
    result.peakFrequencies.reserve(static_cast<size_t>(numPeaks));
    for (int i = 0; i < numPeaks; ++i) {
        float const hz = static_cast<float>(peaks[static_cast<size_t>(i)].second)
                       * static_cast<float>(sampleRate) / kFftSize;
        result.peakFrequencies.push_back(hz);
    }

    return result;
}

juce::String SpectralAnalyzer::format(Result const& r)
{
    auto fmt = [](float v) { return juce::String(v, 2); };

    juce::String s = "spectral: sub=" + fmt(r.sub)
                   + " low="  + fmt(r.low)
                   + " mid="  + fmt(r.midLow)
                   + " hmid=" + fmt(r.midHigh)
                   + " high=" + fmt(r.high);

    if (!r.peakFrequencies.empty()) {
        s += " | peaks:";
        for (auto const hz : r.peakFrequencies)
            s += " " + juce::String(static_cast<int>(hz)) + "Hz";
    }

    return s;
}

} // namespace RepentePd
