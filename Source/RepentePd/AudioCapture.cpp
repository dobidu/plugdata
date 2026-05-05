/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "AudioCapture.h"
#include <cmath>

namespace RepentePd {

void AudioCapture::startCapture(float durationSec, int sampleRate, int numChannels)
{
    // Disarm first so feedAudio won't write during resize
    armed.store(false, std::memory_order_release);
    ready_.store(false, std::memory_order_relaxed);
    writePos.store(0, std::memory_order_relaxed);

    int const target = static_cast<int>(std::ceil(durationSec * static_cast<float>(sampleRate)));
    targetSamples.store(target, std::memory_order_relaxed);
    channels_.store(numChannels, std::memory_order_relaxed);

    captureBuffer.setSize(numChannels, target, false, true, false);

    // Arm last — release fence ensures captureBuffer resize is visible to audio thread
    armed.store(true, std::memory_order_release);
}

void AudioCapture::feedAudio(juce::AudioBuffer<float> const& buffer)
{
    // Wait-free: single atomic load gate, no mutex
    if (!armed.load(std::memory_order_acquire)) return;
    if (ready_.load(std::memory_order_acquire))  return;

    int pos     = writePos.load(std::memory_order_relaxed);
    int target  = targetSamples.load(std::memory_order_relaxed);
    int chans   = channels_.load(std::memory_order_relaxed);
    int srcCh   = buffer.getNumChannels();
    int toCopy  = juce::jmin(buffer.getNumSamples(), target - pos);

    if (toCopy <= 0 || chans <= 0 || srcCh <= 0) return;

    for (int ch = 0; ch < chans; ++ch)
        captureBuffer.copyFrom(ch, pos, buffer, ch % srcCh, 0, toCopy);

    int const newPos = pos + toCopy;
    writePos.store(newPos, std::memory_order_release);

    if (newPos >= target) {
        armed.store(false, std::memory_order_release);
        ready_.store(true,  std::memory_order_release);
    }
}

bool AudioCapture::isReady() const noexcept
{
    return ready_.load(std::memory_order_relaxed);
}

bool AudioCapture::isCapturing() const noexcept
{
    return armed.load(std::memory_order_relaxed) && !ready_.load(std::memory_order_relaxed);
}

juce::AudioBuffer<float> AudioCapture::takeCapture()
{
    if (!ready_.load(std::memory_order_acquire))
        return {};

    juce::AudioBuffer<float> result;
    result.makeCopyOf(captureBuffer);

    // Reset to idle
    armed.store(false, std::memory_order_release);
    ready_.store(false, std::memory_order_relaxed);
    writePos.store(0, std::memory_order_relaxed);
    targetSamples.store(0, std::memory_order_relaxed);
    captureBuffer.setSize(0, 0);

    return result;
}

} // namespace RepentePd
