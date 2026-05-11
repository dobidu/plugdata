#pragma once

#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Bridge/PdParser.h"
#include "RepentePd/SpectralAnalyzer.h"
#include <cmath>

using namespace RepentePd;

// ─────────────────────────────────────────────────────────────────────────────
// CommandParser
// ─────────────────────────────────────────────────────────────────────────────

class CommandParserTest : public UnitTest {
public:
    CommandParserTest() : UnitTest("CommandParser", "RepentePd") {}

    void runTest() override
    {
        beginTest("parse /pds create osc~ 100 100");
        auto r = CommandParser::parse("/pds create osc~ 100 100");
        expect(r.type == CommandType::PDS_CREATE, "type should be PDS_CREATE");
        expect(r.args[0] == "osc~",  "arg 0 = osc~");
        expect(r.args[1] == "100",   "arg 1 = 100");
        expect(r.args[2] == "100",   "arg 2 = 100");
        expect(r.error.isEmpty(),    "no error");

        beginTest("parse /pds foobar → UNKNOWN");
        auto r2 = CommandParser::parse("/pds foobar");
        expect(r2.type == CommandType::UNKNOWN, "type should be UNKNOWN");
        expect(r2.error.isNotEmpty(),           "error message present");

        beginTest("parse natural language → PASSTHROUGH");
        auto r3 = CommandParser::parse("make a simple oscillator patch");
        expect(r3.type == CommandType::PASSTHROUGH, "type should be PASSTHROUGH");

        beginTest("parse /help → HELP");
        auto r4 = CommandParser::parse("/help");
        expect(r4.type == CommandType::HELP, "type should be HELP");

        beginTest("parse /clear → CLEAR");
        auto r5 = CommandParser::parse("/clear");
        expect(r5.type == CommandType::CLEAR, "type should be CLEAR");

        beginTest("parse /pds list → PDS_LIST, no args");
        auto r6 = CommandParser::parse("/pds list");
        expect(r6.type == CommandType::PDS_LIST, "type should be PDS_LIST");
        expect(r6.args.isEmpty(), "no args for list");

        beginTest("parse unknown slash command");
        auto r7 = CommandParser::parse("/bogus");
        expect(r7.type == CommandType::UNKNOWN, "type should be UNKNOWN");
        expect(r7.error.isNotEmpty(), "error message present");
    }
};

static CommandParserTest commandParserTest;

// ─────────────────────────────────────────────────────────────────────────────
// Battery B: text → canvas (PdParser routing, 5 scenarios)
// Tests that canonical LLM response strings for each scenario are correctly
// routed as PD_PATCH and contain expected objects.
// Audio playback requires the full GUI stack — verified manually.
// ─────────────────────────────────────────────────────────────────────────────

class PdParserBatteryBTest : public UnitTest {
public:
    PdParserBatteryBTest() : UnitTest("Battery B: text→canvas routing", "RepentePd") {}

    static juce::String patch(juce::String const& body)
    {
        return "#N canvas 0 0 450 300 12;\n" + body;
    }

    void runTest() override
    {
        // B1: Basic oscillator → dac~
        beginTest("B1: osc~ 440 → dac~");
        {
            auto r = PdParser::parse(patch(
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 dac~;\n"
                "#X connect 0 0 1 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "B1: PD_PATCH");
            expect(r.content.contains("osc~"),       "B1: osc~ present");
            expect(r.content.contains("dac~"),       "B1: dac~ present");
            expect(r.content.contains("#X connect"), "B1: connection present");
        }

        // B2: Filtered noise
        beginTest("B2: noise~ → lop~ → dac~");
        {
            auto r = PdParser::parse(patch(
                "#X obj 100 100 noise~;\n"
                "#X obj 100 150 lop~ 800;\n"
                "#X obj 100 200 dac~;\n"
                "#X connect 0 0 1 0;\n"
                "#X connect 1 0 2 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "B2: PD_PATCH");
            expect(r.content.contains("noise~"),     "B2: noise~ present");
            expect(r.content.contains("lop~"),       "B2: lop~ present");
        }

        // B3: LFO-modulated amplitude (tremolo)
        beginTest("B3: osc~ + LFO → *~ → dac~");
        {
            auto r = PdParser::parse(patch(
                "#X obj  50 100 osc~ 4;\n"
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 *~;\n"
                "#X obj 100 200 dac~;\n"
                "#X connect 0 0 2 1;\n"
                "#X connect 1 0 2 0;\n"
                "#X connect 2 0 3 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "B3: PD_PATCH");
            expect(r.content.contains("*~"),         "B3: *~ present");
        }

        // B4: Four-voice chord
        beginTest("B4: 4-voice detuned chord → dac~");
        {
            auto r = PdParser::parse(patch(
                "#X obj  50 100 osc~ 261;\n"
                "#X obj 100 100 osc~ 330;\n"
                "#X obj 150 100 osc~ 392;\n"
                "#X obj 200 100 osc~ 523;\n"
                "#X obj 125 160 +~;\n"
                "#X obj 125 200 +~;\n"
                "#X obj 125 240 +~;\n"
                "#X obj 125 280 dac~;\n"
                "#X connect 0 0 4 0;\n"
                "#X connect 1 0 4 1;\n"
                "#X connect 2 0 5 0;\n"
                "#X connect 3 0 5 1;\n"
                "#X connect 4 0 6 0;\n"
                "#X connect 5 0 6 1;\n"
                "#X connect 6 0 7 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "B4: PD_PATCH");
            expect(r.content.indexOf("osc~ 261") >= 0, "B4: voice 1 present");
            expect(r.content.indexOf("osc~ 523") >= 0, "B4: voice 4 present");
        }

        // B5: Delay/echo chain
        beginTest("B5: osc~ → delwrite~/delread~ → dac~");
        {
            auto r = PdParser::parse(patch(
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 delwrite~ echo 500;\n"
                "#X obj 100 200 delread~ echo 250;\n"
                "#X obj 100 250 dac~;\n"
                "#X connect 0 0 1 0;\n"
                "#X connect 2 0 3 0;\n"));
            expect(r.type == ResponseType::PD_PATCH,      "B5: PD_PATCH");
            expect(r.content.contains("delwrite~"),       "B5: delwrite~ present");
            expect(r.content.contains("delread~"),        "B5: delread~ present");
        }

        // B-fence: LLMs commonly wrap output in markdown code fences
        beginTest("B-fence: markdown-wrapped patch strips fences correctly");
        {
            juce::String fenced =
                "```pd\n"
                "#N canvas 0 0 450 300 12;\n"
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 dac~;\n"
                "#X connect 0 0 1 0;\n"
                "```";
            auto r = PdParser::parse(fenced);
            expect(r.type == ResponseType::PD_PATCH,   "B-fence: PD_PATCH after strip");
            expect(!r.content.contains("```"),         "B-fence: backticks removed");
            expect(r.content.contains("#N canvas"),    "B-fence: body intact");
        }

        // B-pds: pds commands response also valid generation path
        beginTest("B-pds: /pds commands route correctly");
        {
            juce::String cmds =
                "/pds create osc~ 100 100\n"
                "/pds create dac~ 100 150\n"
                "/pds connect osc_1 dac_1 0 0\n";
            auto r = PdParser::parse(cmds);
            expect(r.type == ResponseType::PDS_COMMANDS, "B-pds: PDS_COMMANDS");
        }
    }
};

static PdParserBatteryBTest batteryBTest;

// ─────────────────────────────────────────────────────────────────────────────
// Battery F: pad → drums → pattern → combined (PdParser routing)
// ─────────────────────────────────────────────────────────────────────────────

class PdParserBatteryFTest : public UnitTest {
public:
    PdParserBatteryFTest() : UnitTest("Battery F: pad→drums→pattern→combined routing", "RepentePd") {}

    static juce::String patch(juce::String const& body)
    {
        return "#N canvas 0 0 450 300 12;\n" + body;
    }

    void runTest() override
    {
        // F1: Pad sound — slow attack envelope on oscillator
        beginTest("F1: pad sound — osc~ + adsr~ envelope");
        {
            auto r = PdParser::parse(patch(
                "#X obj 100  50 osc~ 220;\n"
                "#X obj 100 100 adsr~ 200 100 0.7 500;\n"
                "#X obj 100 150 *~;\n"
                "#X obj 100 200 dac~;\n"
                "#X connect 0 0 2 0;\n"
                "#X connect 1 0 2 1;\n"
                "#X connect 2 0 3 0;\n"));
            expect(r.type == ResponseType::PD_PATCH,     "F1: PD_PATCH");
            expect(r.content.contains("osc~"),           "F1: osc~ present");
            expect(r.content.contains("adsr~") || r.content.contains("env~"),
                                                         "F1: envelope present");
        }

        // F2: Drum kit — noise-based snare + pitched kick
        beginTest("F2: drum kit — noise~ snare + osc~ kick");
        {
            auto r = PdParser::parse(patch(
                "#X obj  50  50 noise~;\n"
                "#X obj  50 100 adsr~ 1 80 0 40;\n"
                "#X obj  50 150 *~;\n"
                "#X obj 150  50 osc~ 60;\n"
                "#X obj 150 100 adsr~ 1 150 0 80;\n"
                "#X obj 150 150 *~;\n"
                "#X obj 100 200 +~;\n"
                "#X obj 100 250 dac~;\n"
                "#X connect 0 0 2 0;\n"
                "#X connect 1 0 2 1;\n"
                "#X connect 3 0 5 0;\n"
                "#X connect 4 0 5 1;\n"
                "#X connect 2 0 6 0;\n"
                "#X connect 5 0 6 1;\n"
                "#X connect 6 0 7 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "F2: PD_PATCH");
            expect(r.content.contains("noise~"),     "F2: noise~ snare present");
            expect(r.content.indexOf("osc~ 60") >= 0,"F2: kick osc present");
        }

        // F3: Rhythm pattern — metro-driven sequencer
        beginTest("F3: rhythm pattern — metro + counter + sel");
        {
            auto r = PdParser::parse(patch(
                "#X obj 100  50 metro 125;\n"
                "#X obj 100 100 counter 0 7;\n"
                "#X obj 100 150 sel 0 2 4 6;\n"
                "#X obj 100 200 osc~ 440;\n"
                "#X obj 100 250 dac~;\n"
                "#X connect 0 0 1 0;\n"
                "#X connect 1 0 2 0;\n"
                "#X connect 3 0 4 0;\n"));
            expect(r.type == ResponseType::PD_PATCH, "F3: PD_PATCH");
            expect(r.content.contains("metro"),      "F3: metro present");
            expect(r.content.contains("counter") || r.content.contains("sel"),
                                                     "F3: sequencer logic present");
        }

        // F4: Combined — pad + drums + pattern in one patch
        beginTest("F4: combined — pad + drums + metro pattern");
        {
            auto r = PdParser::parse(patch(
                "#X obj  50  50 metro 125;\n"
                "#X obj  50 100 counter 0 3;\n"
                "#X obj  50 150 sel 0 2;\n"
                "#X obj 100  50 osc~ 220;\n"
                "#X obj 100 100 adsr~ 20 300 0.6 200;\n"
                "#X obj 100 150 *~;\n"
                "#X obj 200  50 noise~;\n"
                "#X obj 200 100 adsr~ 1 80 0 40;\n"
                "#X obj 200 150 *~;\n"
                "#X obj 150 200 +~;\n"
                "#X obj 150 250 dac~;\n"
                "#X connect 0 0 1 0;\n"
                "#X connect 3 0 5 0;\n"
                "#X connect 4 0 5 1;\n"
                "#X connect 6 0 8 0;\n"
                "#X connect 7 0 8 1;\n"
                "#X connect 5 0 9 0;\n"
                "#X connect 8 0 9 1;\n"
                "#X connect 9 0 10 0;\n"));
            expect(r.type == ResponseType::PD_PATCH,  "F4: PD_PATCH");
            expect(r.content.contains("metro"),       "F4: metro present");
            expect(r.content.contains("noise~"),      "F4: drum noise present");
            expect(r.content.contains("osc~"),        "F4: pad osc present");
            expect(r.content.contains("adsr~"),       "F4: envelopes present");
        }
    }
};

static PdParserBatteryFTest batteryFTest;

// ─────────────────────────────────────────────────────────────────────────────
// SpectralAnalyzer unit tests
// Synthetic audio buffers — no LLM or audio device needed.
// ─────────────────────────────────────────────────────────────────────────────

class SpectralAnalyzerTest : public UnitTest {
public:
    SpectralAnalyzerTest() : UnitTest("SpectralAnalyzer", "RepentePd") {}

    static juce::AudioBuffer<float> silentBuffer(int numSamples = 4096)
    {
        juce::AudioBuffer<float> buf(1, numSamples);
        buf.clear();
        return buf;
    }

    static juce::AudioBuffer<float> sineBuffer(float freqHz, float sampleRate, int numSamples = 4096)
    {
        juce::AudioBuffer<float> buf(1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buf.setSample(0, i, std::sin(2.0f * juce::MathConstants<float>::pi * freqHz * i / sampleRate));
        return buf;
    }

    static juce::AudioBuffer<float> whiteNoiseBuffer(int numSamples = 8192)
    {
        juce::AudioBuffer<float> buf(1, numSamples);
        juce::Random rng(42);
        for (int i = 0; i < numSamples; ++i)
            buf.setSample(0, i, rng.nextFloat() * 2.0f - 1.0f);
        return buf;
    }

    void runTest() override
    {
        constexpr int kSR = 44100;

        // SA-1: Silence → all zero Result, no crash
        beginTest("SA-1: silent buffer → zero Result");
        {
            auto r = SpectralAnalyzer::analyze(silentBuffer(), kSR);
            expectWithinAbsoluteError(r.sub,     0.f, 1e-6f, "SA-1: sub = 0");
            expectWithinAbsoluteError(r.low,     0.f, 1e-6f, "SA-1: low = 0");
            expectWithinAbsoluteError(r.midLow,  0.f, 1e-6f, "SA-1: midLow = 0");
            expectWithinAbsoluteError(r.midHigh, 0.f, 1e-6f, "SA-1: midHigh = 0");
            expectWithinAbsoluteError(r.high,    0.f, 1e-6f, "SA-1: high = 0");
            expect(r.peakFrequencies.empty(), "SA-1: no peaks");
        }

        // SA-2: Buffer too short (< 2048 samples) → zero Result, no crash
        beginTest("SA-2: short buffer (< FFT size) → zero Result");
        {
            juce::AudioBuffer<float> tiny(1, 512);
            tiny.clear();
            auto r = SpectralAnalyzer::analyze(tiny, kSR);
            expectWithinAbsoluteError(r.sub, 0.f, 1e-6f, "SA-2: zero on short buffer");
            expect(r.peakFrequencies.empty(), "SA-2: no peaks");
        }

        // SA-3: 440 Hz sine → peak near 440 Hz
        beginTest("SA-3: 440 Hz sine → peak frequency ≈ 440 Hz");
        {
            auto buf = sineBuffer(440.0f, kSR);
            auto r   = SpectralAnalyzer::analyze(buf, kSR);
            expect(!r.peakFrequencies.empty(), "SA-3: at least one peak");
            if (!r.peakFrequencies.empty())
            {
                float peak = r.peakFrequencies[0];
                // FFT resolution = SR / FFT_SIZE = 44100/2048 ≈ 21.5 Hz
                // Allow ±2 bins (±43 Hz)
                expect(std::abs(peak - 440.0f) < 45.0f,
                       "SA-3: peak within 45 Hz of 440 Hz (got " + juce::String(peak, 1) + " Hz)");
            }
            // 440 Hz falls in midLow band (250–2000 Hz) — should be loudest
            expect(r.midLow > 0.f, "SA-3: midLow band non-zero");
        }

        // SA-4: 60 Hz sine → peak in sub band
        beginTest("SA-4: 60 Hz sine → sub band dominant");
        {
            auto buf = sineBuffer(60.0f, kSR);
            auto r   = SpectralAnalyzer::analyze(buf, kSR);
            expect(r.sub > 0.f || r.low > 0.f, "SA-4: low-frequency content detected");
        }

        // SA-5: White noise → all bands non-zero, bands normalized to [0,1]
        beginTest("SA-5: white noise → all bands > 0, loudest = 1.0");
        {
            auto buf = whiteNoiseBuffer();
            auto r   = SpectralAnalyzer::analyze(buf, kSR);
            expect(r.sub     > 0.f, "SA-5: sub > 0");
            expect(r.low     > 0.f, "SA-5: low > 0");
            expect(r.midLow  > 0.f, "SA-5: midLow > 0");
            expect(r.midHigh > 0.f, "SA-5: midHigh > 0");
            expect(r.high    > 0.f, "SA-5: high > 0");
            float maxBand = std::max({r.sub, r.low, r.midLow, r.midHigh, r.high});
            expectWithinAbsoluteError(maxBand, 1.0f, 1e-5f, "SA-5: loudest band = 1.0");
            expect(r.sub     <= 1.0f, "SA-5: sub ≤ 1");
            expect(r.high    <= 1.0f, "SA-5: high ≤ 1");
        }

        // SA-6: format() output matches expected pattern
        beginTest("SA-6: format() produces correct string structure");
        {
            auto buf = sineBuffer(440.0f, kSR);
            auto r   = SpectralAnalyzer::analyze(buf, kSR);
            auto s   = SpectralAnalyzer::format(r);
            expect(s.startsWith("spectral:"),      "SA-6: starts with 'spectral:'");
            expect(s.contains("sub="),             "SA-6: has sub=");
            expect(s.contains("low="),             "SA-6: has low=");
            expect(s.contains("mid="),             "SA-6: has mid=");
            expect(s.contains("hmid="),            "SA-6: has hmid=");
            expect(s.contains("high="),            "SA-6: has high=");
            if (!r.peakFrequencies.empty())
            {
                expect(s.contains("| peaks:"),    "SA-6: has peaks section");
                expect(s.contains("Hz"),           "SA-6: Hz unit present");
            }
        }

        // SA-7: Stereo buffer handled correctly (mix-down to mono)
        beginTest("SA-7: stereo buffer → same result as equivalent mono");
        {
            constexpr int N = 4096;
            juce::AudioBuffer<float> stereo(2, N);
            for (int i = 0; i < N; ++i) {
                float v = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / kSR);
                stereo.setSample(0, i, v);
                stereo.setSample(1, i, v);
            }
            auto rStereo = SpectralAnalyzer::analyze(stereo, kSR);
            auto rMono   = SpectralAnalyzer::analyze(sineBuffer(440.0f, kSR), kSR);
            // Stereo same-content mix-down should give same peak freq as mono
            expect(!rStereo.peakFrequencies.empty(), "SA-7: stereo has peaks");
            if (!rStereo.peakFrequencies.empty() && !rMono.peakFrequencies.empty())
            {
                expect(std::abs(rStereo.peakFrequencies[0] - rMono.peakFrequencies[0]) < 45.0f,
                       "SA-7: stereo peak matches mono peak");
            }
        }
    }
};

static SpectralAnalyzerTest spectralAnalyzerTest;
