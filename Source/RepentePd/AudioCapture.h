/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace RepentePd {

// Thread-safe audio capture: audio thread pushes samples (wait-free);
// message thread arms capture and retrieves results.
class AudioCapture {
public:
    // Message thread: arm capture for durationSec seconds.
    // Resets any prior capture. Safe while processBlock runs.
    void startCapture(float durationSec, int sampleRate, int numChannels);

    // Audio thread: feed output samples. Wait-free.
    // No-op if not armed or already ready.
    void feedAudio(juce::AudioBuffer<float> const& buffer);

    // Message thread: true once enough samples have been accumulated.
    [[nodiscard]] bool isReady() const noexcept;

    // Message thread: true while capture is armed but not yet complete.
    [[nodiscard]] bool isCapturing() const noexcept;

    // Message thread: move captured buffer out and reset to idle.
    // Returns empty buffer if not ready.
    [[nodiscard]] juce::AudioBuffer<float> takeCapture();

private:
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int>  writePos      { 0 };
    std::atomic<int>  targetSamples { 0 };
    std::atomic<int>  channels_     { 0 };
    std::atomic<bool> armed         { false };
    std::atomic<bool> ready_        { false };
};

} // namespace RepentePd
