#pragma once

#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Bridge/PdParser.h"
#include "RepentePd/Bridge/OpenAIProvider.h"
#include "RepentePd/Bridge/AnthropicProvider.h"
#include "RepentePd/Bridge/PresetLoader.h"
#include "RepentePd/Bridge/RepenteClient.h"
#include "RepentePd/Bridge/PromptNormalizer.h"
#include "RepentePd/SpectralAnalyzer.h"
#include <cmath>
#include <nlohmann/json.hpp>

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
// No-patch detection: responses that must NOT reach the Lua engine.
// Regression guard for the `unexpected symbol near 'not'` console errors —
// LUA_BLOCK used to be an unconditional fallback, so prose was executed.
// ─────────────────────────────────────────────────────────────────────────────

class PdParserNoPatchTest : public UnitTest {
public:
    PdParserNoPatchTest() : UnitTest("PdParser: no-patch detection", "RepentePd") {}

    void runTest() override
    {
        beginTest("NP-1: plain prose → NO_PATCH/Prose, not LUA_BLOCK");
        {
            auto r = PdParser::parse(
                "I can't create that patch because the sample rate is not specified. "
                "Could you tell me which rate you want to use?");
            expect(r.type == ResponseType::NO_PATCH,        "NP-1: NO_PATCH");
            expect(r.reason == NoPatchReason::Prose,        "NP-1: reason Prose");
            expect(PdParser::describe(r.reason).isNotEmpty(), "NP-1: has description");
        }

        beginTest("NP-2: prose containing 'not' and braces stays NO_PATCH");
        {
            // The exact shapes that produced Lua syntax errors in the console.
            auto a = PdParser::parse("This is not a valid request for a patch.");
            auto b = PdParser::parse("Use the {osc~} object with a rate = 440 setting.");
            expect(a.type == ResponseType::NO_PATCH, "NP-2a: 'not' prose is NO_PATCH");
            expect(b.type == ResponseType::NO_PATCH, "NP-2b: brace prose is NO_PATCH");
        }

        beginTest("NP-3: patch fragment without #N canvas → PatchFragment");
        {
            auto r = PdParser::parse(
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 dac~;\n"
                "#X connect 0 0 1 0;\n");
            expect(r.type == ResponseType::NO_PATCH,           "NP-3: NO_PATCH");
            expect(r.reason == NoPatchReason::PatchFragment,   "NP-3: reason PatchFragment");
        }

        beginTest("NP-4: empty / fence-only response → Empty");
        {
            auto r = PdParser::parse("```\n\n```");
            expect(r.type == ResponseType::NO_PATCH,   "NP-4: NO_PATCH");
            expect(r.reason == NoPatchReason::Empty,   "NP-4: reason Empty");
        }

        beginTest("NP-5: real Lua still routes to LUA_BLOCK");
        {
            auto viaApi = PdParser::parse(
                "for i = 1, 4 do\n"
                "  pds.create('osc~', 100, i * 40)\n"
                "end");
            expect(viaApi.type == ResponseType::LUA_BLOCK, "NP-5a: pds.* → LUA_BLOCK");

            auto viaFence = PdParser::parse("```lua\nprint('hello')\n```");
            expect(viaFence.type == ResponseType::LUA_BLOCK, "NP-5b: ```lua tag → LUA_BLOCK");

            auto viaShape = PdParser::parse("local rate = 440\n");
            expect(viaShape.type == ResponseType::LUA_BLOCK, "NP-5c: local assignment → LUA_BLOCK");
        }

        beginTest("NP-6: valid patch and /pds are unaffected");
        {
            auto patch = PdParser::parse(
                "#N canvas 0 0 450 300 12;\n#X obj 100 100 osc~ 440;\n");
            expect(patch.type == ResponseType::PD_PATCH,      "NP-6a: PD_PATCH");
            expect(patch.reason == NoPatchReason::None,       "NP-6a: no reason set");

            auto cmds = PdParser::parse("/pds create osc~ 100 100\n");
            expect(cmds.type == ResponseType::PDS_COMMANDS,   "NP-6b: PDS_COMMANDS");
        }

        beginTest("NP-7: prose preamble before a fenced patch still yields PD_PATCH");
        {
            auto r = PdParser::parse(
                "Here is a simple sine tone:\n\n"
                "#N canvas 0 0 450 300 12;\n"
                "#X obj 100 100 osc~ 440;\n");
            expect(r.type == ResponseType::PD_PATCH, "NP-7: patch wins over surrounding prose");
        }
    }
};

static PdParserNoPatchTest noPatchTest;

// ─────────────────────────────────────────────────────────────────────────────
// Patch extraction: a valid patch plus trailing junk must yield only the patch.
// Regression guard for the "no such object" cascade — PdParser::parse used a
// `contains("#N canvas")` test and returned the whole response, so SuperCollider
// appended after a patch reached openPatch() and each line became a bogus object.
// ─────────────────────────────────────────────────────────────────────────────

class PdParserExtractTest : public UnitTest {
public:
    PdParserExtractTest() : UnitTest("PdParser: patch extraction", "RepentePd") {}

    void runTest() override
    {
        beginTest("EX-1: trailing SuperCollider after a valid patch is dropped");
        {
            // Verbatim from the reported console session (repente:v0.5).
            auto r = PdParser::parse(
                "#N canvas 345 127 458 267 10;\n"
                "#X obj 108 69 osc~ 440;\n"
                "#X obj 108 131 dac~;\n"
                "#X obj 120 107 bp~ 1000 10;\n"
                "#X connect 0 0 4 0;\n"
                "\n"
                "// Now add another lowpass with a cutoff of 600 cycles\n"
                "(\n"
                "Ndef('lpf', { arg freq; \n"
                "\tvar in = InFeedback.ar(0,2);\n"
                "\tOut.ar(0,out);\n"
                "})\n"
                ");\n"
                "MIDIIn.connectControls;\n");
            expect(r.type == ResponseType::PD_PATCH,        "EX-1: still PD_PATCH");
            expect(!r.content.contains("Ndef"),             "EX-1: SuperCollider dropped");
            expect(!r.content.contains("MIDIIn"),           "EX-1: trailing call dropped");
            expect(!r.content.contains("//"),               "EX-1: comment dropped");
            expect(r.content.contains("bp~ 1000 10"),       "EX-1: patch body kept");
            expect(r.content.contains("#N canvas"),         "EX-1: header kept");
            expect(r.discardedLines == 8,                   "EX-1: counted 8 dropped lines");
        }

        beginTest("EX-2: leading prose before the header is dropped");
        {
            auto r = PdParser::parse(
                "Here is a simple sine tone:\n"
                "\n"
                "#N canvas 0 0 450 300 12;\n"
                "#X obj 100 100 osc~ 440;\n");
            expect(r.type == ResponseType::PD_PATCH,           "EX-2: PD_PATCH");
            expect(r.content.startsWith("#N canvas"),          "EX-2: starts at header");
            expect(!r.content.contains("simple sine tone"),    "EX-2: preamble dropped");
            expect(r.discardedLines == 1,                      "EX-2: counted 1 dropped line");
        }

        beginTest("EX-3: a clean patch is passed through untouched");
        {
            juce::String const clean =
                "#N canvas 0 0 450 300 12;\n"
                "#X obj 100 100 osc~ 440;\n"
                "#X obj 100 150 dac~;\n"
                "#X connect 0 0 1 0;";
            auto r = PdParser::parse(clean);
            expect(r.type == ResponseType::PD_PATCH, "EX-3: PD_PATCH");
            expect(r.content == clean,               "EX-3: byte-identical");
            expect(r.discardedLines == 0,            "EX-3: nothing discarded");
        }

        beginTest("EX-4: a record wrapping across lines survives");
        {
            // #X text without a terminating ';' continues on the next line; the
            // walker must not treat the continuation as the end of the patch.
            auto r = PdParser::parse(
                "#N canvas 0 0 450 300 12;\n"
                "#X text 20 20 this comment wraps\n"
                "onto a second line;\n"
                "#X obj 100 100 osc~ 440;\n");
            expect(r.content.contains("onto a second line"), "EX-4: continuation kept");
            expect(r.content.contains("osc~ 440"),           "EX-4: record after it kept");
            expect(r.discardedLines == 0,                    "EX-4: nothing discarded");
        }

        beginTest("EX-5: blank lines between records don't end the patch");
        {
            auto r = PdParser::parse(
                "#N canvas 0 0 450 300 12;\n"
                "#X obj 100 100 osc~ 440;\n"
                "\n"
                "#X obj 100 150 dac~;\n");
            expect(r.content.contains("dac~"),  "EX-5: record after blank kept");
            expect(r.discardedLines == 0,       "EX-5: blanks aren't counted");
        }
    }
};

static PdParserExtractTest extractTest;

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

// ─────────────────────────────────────────────────────────────────────────────
// OpenAIProvider — body/header/parse
// ─────────────────────────────────────────────────────────────────────────────

class OpenAIProviderTest : public UnitTest {
public:
    OpenAIProviderTest() : UnitTest("OpenAIProvider", "RepentePd") {}

    void runTest() override
    {
        using json = nlohmann::json;
        OpenAIProvider p;

        beginTest("endpoints + name");
        expect(p.name() == "openai");
        expect(p.chatEndpointPath() == "/v1/chat/completions");
        expect(p.pingEndpointPath() == "/v1/models");

        beginTest("buildHeaders: no key → no Authorization");
        {
            std::vector<std::pair<std::string, std::string>> h;
            p.buildHeaders("", h);
            bool hasAuth = false;
            for (auto const& kv : h) if (kv.first == "Authorization") hasAuth = true;
            expect(!hasAuth, "no Authorization header when key empty");
        }

        beginTest("buildHeaders: with key → Bearer auth");
        {
            std::vector<std::pair<std::string, std::string>> h;
            p.buildHeaders("sk-test", h);
            bool found = false;
            for (auto const& kv : h)
                if (kv.first == "Authorization" && kv.second == "Bearer sk-test") found = true;
            expect(found, "Bearer sk-test present");
        }

        beginTest("buildBody: shape unchanged from pre-refactor");
        {
            LlmRequest req;
            req.model     = "gpt-4o";
            req.maxTokens = 0;     // OpenAI: omit when 0
            req.messages  = {{"system", "you are helpful"}, {"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(body["model"] == "gpt-4o", "model preserved");
            expect(body["messages"].is_array() && body["messages"].size() == 2, "2 messages");
            expect(body["messages"][0]["role"] == "system", "system stays in array");
            expect(body["messages"][1]["role"] == "user",   "user message follows");
            expect(body.contains("stream") && body["stream"] == false, "stream:false");
            expect(!body.contains("max_tokens"), "max_tokens omitted when 0");
        }

        beginTest("buildBody: max_tokens included when > 0");
        {
            LlmRequest req;
            req.model     = "gpt-4o";
            req.maxTokens = 1024;
            req.messages  = {{"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(body.contains("max_tokens") && body["max_tokens"] == 1024);
        }

        beginTest("parseResponse: success");
        {
            std::string body = R"({"choices":[{"message":{"content":"hello"}}]})";
            expect(p.parseResponse(body) == "hello");
        }

        beginTest("parseResponse: API error surfaced");
        {
            std::string body = R"({"error":{"message":"invalid key","type":"auth"}})";
            auto r = p.parseResponse(body);
            expect(r.startsWith("error:"));
            expect(r.contains("invalid key"));
        }

        beginTest("parseResponse: malformed JSON");
        {
            auto r = p.parseResponse("not json {");
            expect(r.startsWith("error:"));
            expect(r.contains("invalid JSON"));
        }

        beginTest("parsePingResponse: 200 with data → models count");
        {
            bool ok = false;
            auto msg = p.parsePingResponse(R"({"data":[{"id":"a"},{"id":"b"}]})", 200, ok);
            expect(ok);
            expect(msg.contains("2"));
        }

        beginTest("parsePingResponse: non-200 → ok=false");
        {
            bool ok = true;
            auto msg = p.parsePingResponse("", 401, ok);
            expect(!ok);
            expect(msg.contains("401"));
        }
    }
};
static OpenAIProviderTest openAIProviderTest;

// ─────────────────────────────────────────────────────────────────────────────
// AnthropicProvider — body/header/parse
// ─────────────────────────────────────────────────────────────────────────────

class AnthropicProviderTest : public UnitTest {
public:
    AnthropicProviderTest() : UnitTest("AnthropicProvider", "RepentePd") {}

    void runTest() override
    {
        using json = nlohmann::json;
        AnthropicProvider p;

        beginTest("endpoints + name");
        expect(p.name() == "anthropic");
        expect(p.chatEndpointPath() == "/v1/messages");

        beginTest("buildHeaders: x-api-key + anthropic-version");
        {
            std::vector<std::pair<std::string, std::string>> h;
            p.buildHeaders("sk-ant-test", h);
            bool hasKey = false, hasVer = false, hasNoBearer = true;
            for (auto const& kv : h) {
                if (kv.first == "x-api-key" && kv.second == "sk-ant-test")              hasKey = true;
                if (kv.first == "anthropic-version" && !kv.second.empty())              hasVer = true;
                if (kv.first == "Authorization")                                        hasNoBearer = false;
            }
            expect(hasKey, "x-api-key present");
            expect(hasVer, "anthropic-version present");
            expect(hasNoBearer, "no Authorization header (Anthropic uses x-api-key)");
        }

        beginTest("buildBody: system extracted from messages → top-level");
        {
            LlmRequest req;
            req.model     = "claude-sonnet-4-6";
            req.maxTokens = 2048;
            req.messages  = {{"system", "canvas state"}, {"user", "make sine"}};
            json body = json::parse(p.buildBody(req));
            expect(body.contains("system"), "system field at top level");
            // system is array of content blocks (the shape that carries cache_control)
            expect(body["system"].is_array() && body["system"].size() == 1,
                   "system is array with one text block");
            expect(body["system"][0]["text"] == "canvas state", "text preserved");
            expect(body["messages"].is_array() && body["messages"].size() == 1,
                   "only user message in messages array");
            expect(body["messages"][0]["role"] == "user");
            expect(body.contains("max_tokens") && body["max_tokens"] == 2048,
                   "max_tokens REQUIRED");
        }

        beginTest("buildBody: multiple system messages concatenated");
        {
            LlmRequest req;
            req.model    = "claude-opus-4-7";
            req.messages = {{"system", "alpha"}, {"system", "beta"}, {"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(body["system"].is_array() && body["system"].size() == 1,
                   "system collapsed into one content block");
            std::string sys = body["system"][0]["text"].get<std::string>();
            expect(sys.find("alpha") != std::string::npos);
            expect(sys.find("beta")  != std::string::npos);
            expect(body["messages"].size() == 1);
        }

        beginTest("buildBody: consecutive same-role messages collapsed");
        {
            LlmRequest req;
            req.model    = "claude-haiku-4-5-20251001";
            req.messages = {{"user", "first"}, {"user", "second"}, {"assistant", "ok"}};
            json body = json::parse(p.buildBody(req));
            expect(body["messages"].size() == 2, "consecutive users collapsed");
            std::string c0 = body["messages"][0]["content"].get<std::string>();
            expect(c0.find("first")  != std::string::npos);
            expect(c0.find("second") != std::string::npos);
        }

        beginTest("buildBody: system block carries cache_control: ephemeral");
        {
            LlmRequest req;
            req.model    = "claude-sonnet-4-6";
            req.messages = {{"system", "canvas state"}, {"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(body.contains("system") && body["system"].is_array(),
                   "system is array of content blocks (required for cache_control)");
            expect(body["system"].size() == 1, "one system block");
            auto const& blk = body["system"][0];
            expect(blk["type"] == "text", "block is text type");
            expect(blk["text"] == "canvas state", "text preserved");
            expect(blk.contains("cache_control"), "cache_control present");
            expect(blk["cache_control"]["type"] == "ephemeral", "ephemeral TTL");
        }

        beginTest("buildBody: no system field when none provided");
        {
            LlmRequest req;
            req.model    = "claude-sonnet-4-6";
            req.messages = {{"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(!body.contains("system"), "system omitted when empty");
        }

        beginTest("buildBody: max_tokens defaults to 4096 when unset");
        {
            LlmRequest req;
            req.model    = "claude-sonnet-4-6";
            req.maxTokens = 0;
            req.messages = {{"user", "hi"}};
            json body = json::parse(p.buildBody(req));
            expect(body["max_tokens"] == 4096, "default when 0");
        }

        beginTest("parseResponse: text block extracted");
        {
            std::string body = R"({"content":[{"type":"text","text":"hello world"}]})";
            expect(p.parseResponse(body) == "hello world");
        }

        beginTest("parseResponse: multiple text blocks concatenated");
        {
            std::string body = R"({"content":[
                {"type":"text","text":"alpha "},
                {"type":"text","text":"beta"}
            ]})";
            auto r = p.parseResponse(body);
            expect(r.contains("alpha"));
            expect(r.contains("beta"));
        }

        beginTest("parseResponse: API error → readable message");
        {
            std::string body = R"({"type":"error","error":{"type":"invalid_request_error","message":"bad model"}})";
            auto r = p.parseResponse(body);
            expect(r.startsWith("error:"));
            expect(r.contains("bad model"));
        }

        beginTest("parseResponse: malformed JSON");
        {
            auto r = p.parseResponse("not json");
            expect(r.startsWith("error:"));
        }
    }
};
static AnthropicProviderTest anthropicProviderTest;

// ─────────────────────────────────────────────────────────────────────────────
// PresetLoader — defaults + parse + merge
// ─────────────────────────────────────────────────────────────────────────────

class PresetLoaderTest : public UnitTest {
public:
    PresetLoaderTest() : UnitTest("PresetLoader", "RepentePd") {}

    static Preset const* findInList(std::vector<Preset> const& v, juce::String const& name)
    {
        for (auto const& p : v) if (p.name == name) return &p;
        return nullptr;
    }

    void runTest() override
    {
        beginTest("bundled defaults parse cleanly");
        {
            auto defaults = PresetLoader::loadAll();
            expect(defaults.size() >= 6, "at least 6 default presets");
            expect(findInList(defaults, "claude-opus")   != nullptr, "claude-opus present");
            expect(findInList(defaults, "claude-sonnet") != nullptr, "claude-sonnet present");
            expect(findInList(defaults, "claude-haiku")  != nullptr, "claude-haiku present");
            expect(findInList(defaults, "gpt-4o")        != nullptr, "gpt-4o present");
            expect(findInList(defaults, "ollama")        != nullptr, "ollama present");
            expect(findInList(defaults, "repente")       != nullptr, "repente present");
        }

        beginTest("default Claude presets carry expected model IDs");
        {
            auto defaults = PresetLoader::loadAll();
            auto* opus    = findInList(defaults, "claude-opus");
            auto* sonnet  = findInList(defaults, "claude-sonnet");
            auto* haiku   = findInList(defaults, "claude-haiku");
            if (opus)   expect(opus->model.contains("opus-4-7"),         "opus model id");
            if (sonnet) expect(sonnet->model.contains("sonnet-4-6"),     "sonnet model id");
            if (haiku)  expect(haiku->model.contains("haiku-4-5"),       "haiku model id");
            if (opus)   expect(opus->provider == "anthropic");
            if (opus)   expect(opus->keyEnv   == "ANTHROPIC_API_KEY");
            if (opus)   expect(opus->requiresKey == true);
        }

        beginTest("find for known + unknown");
        {
            bool found = false;
            auto p = PresetLoader::findOrEmpty("claude-sonnet", found);
            expect(found);
            expect(p.url.containsIgnoreCase("anthropic.com"));

            bool foundNo = true;
            auto missing = PresetLoader::findOrEmpty("does-not-exist", foundNo);
            expect(!foundNo);
            expect(missing.name.isEmpty());
        }

        beginTest("local presets are localhost");
        {
            auto defaults = PresetLoader::loadAll();
            auto* ollama  = findInList(defaults, "ollama");
            auto* repente = findInList(defaults, "repente");
            if (ollama)  expect(ollama->url.contains("localhost"));
            if (repente) expect(repente->url.contains("localhost"));
            if (ollama)  expect(!ollama->requiresKey);
        }
    }
};
static PresetLoaderTest presetLoaderTest;

// ─────────────────────────────────────────────────────────────────────────────
// PromptNormalizer
// ─────────────────────────────────────────────────────────────────────────────

class PromptNormalizerTest : public UnitTest {
public:
    PromptNormalizerTest() : UnitTest("PromptNormalizer", "RepentePd") {}

    void runTest() override
    {
        using Intent = PromptNormalizer::Intent;

        beginTest("training-set prompts pass through byte-identical");
        {
            // The five phrasings the model was trained on — the rewriter must
            // never touch a prompt that already names the language.
            char const* paperPrompts[] = {
                "Write a Pure Data patch for a simple FM synthesizer with a sine carrier modulated by another sine oscillator.",
                "Create a Pure Data patch that implements Karplus-Strong plucked string synthesis.",
                "Explain Pd FM to beginner",
                "Convert SC Saw+RLPF to Pd",
                "What does Pd noise+bp~ do?",
            };
            for (auto const* p : paperPrompts) {
                expect(PromptNormalizer::classify(p) == Intent::AlreadyQualified, p);
                expect(PromptNormalizer::normalize(p) == juce::String(p), p);
            }
        }

        beginTest("qualifier match is word-bounded");
        {
            // "pd" inside another word must not count as already-qualified.
            expect(PromptNormalizer::classify("update the filter") != Intent::AlreadyQualified);
            expect(PromptNormalizer::classify("speed things up")   != Intent::AlreadyQualified);
            expect(PromptNormalizer::classify("make it plugdata")  == Intent::AlreadyQualified);
        }

        beginTest("create: verb stripped, reframed as a patch request");
        {
            expect(PromptNormalizer::classify("make an FM synth") == Intent::Create);
            expect(PromptNormalizer::normalize("make an FM synth")
                   == "Write a Pure Data patch for an FM synth.");
            // Stacked verb phrase + indirect object both get stripped.
            expect(PromptNormalizer::normalize("please write me a granular sampler")
                   == "Write a Pure Data patch for a granular sampler.");
            // Bare noun phrase, no leading verb.
            expect(PromptNormalizer::normalize("FM synth with two sine oscillators")
                   == "Write a Pure Data patch for FM synth with two sine oscillators.");
            // Relative clause takes the "Create a Pure Data patch that ..." form.
            expect(PromptNormalizer::normalize("that implements Karplus-Strong")
                   == "Create a Pure Data patch that implements Karplus-Strong.");
        }

        beginTest("modify: add-form vs general frame");
        {
            expect(PromptNormalizer::classify("add reverb") == Intent::Modify);
            expect(PromptNormalizer::normalize("add reverb")
                   == "Add reverb to the Pure Data patch.");
            // Already has its own "to ..." target, so the add-form would garble it.
            expect(PromptNormalizer::normalize("connect the osc to the dac")
                   == "In this Pure Data patch, connect the osc to the dac.");
        }

        beginTest("analyze: qualifies the noun in place");
        {
            expect(PromptNormalizer::classify("what does this patch do?") == Intent::Analyze);
            expect(PromptNormalizer::normalize("what does this patch do?")
                   == "What does this Pure Data patch do?");
            expect(PromptNormalizer::normalize("what does noise+bp~ do?")
                   == "In Pure Data, what does noise+bp~ do?");
            // A trailing "?" is enough — without it this would classify as Create.
            expect(PromptNormalizer::classify("can you tell me about bp~?") == Intent::Analyze);
        }

        beginTest("convert: target language appended");
        {
            expect(PromptNormalizer::classify("convert this SC saw") == Intent::Convert);
            expect(PromptNormalizer::normalize("convert this SC saw")
                   == "Convert this SC saw to Pure Data.");
        }

        beginTest("empty and whitespace-only prompts are returned unchanged");
        {
            expect(PromptNormalizer::normalize("")     == "");
            expect(PromptNormalizer::normalize("   ")  == "   ");
        }
    }
};
static PromptNormalizerTest promptNormalizerTest;

// ─────────────────────────────────────────────────────────────────────────────
// RepenteClient URL composition
// ─────────────────────────────────────────────────────────────────────────────

class RepenteClientUrlTest : public UnitTest {
public:
    RepenteClientUrlTest() : UnitTest("RepenteClientUrl", "RepentePd") {}

    void runTest() override
    {
        juce::String const anthropic = "/v1/messages";
        juce::String const openai    = "/v1/chat/completions";

        beginTest("bare host + endpoint");
        {
            expect(RepenteClient::buildUrl("https://api.anthropic.com", anthropic)
                   == "https://api.anthropic.com/v1/messages");
            expect(RepenteClient::buildUrl("http://localhost:11434", openai)
                   == "http://localhost:11434/v1/chat/completions");
        }

        beginTest("base already carrying /v1 does not double it");
        {
            // Regression: this produced /v1/v1/chat/completions, which 404s.
            expect(RepenteClient::buildUrl("https://api.anthropic.com/v1", anthropic)
                   == "https://api.anthropic.com/v1/messages");
            expect(RepenteClient::buildUrl("https://api.openai.com/v1", openai)
                   == "https://api.openai.com/v1/chat/completions");
        }

        beginTest("trailing slashes are ignored");
        {
            expect(RepenteClient::buildUrl("https://api.anthropic.com/", anthropic)
                   == "https://api.anthropic.com/v1/messages");
            expect(RepenteClient::buildUrl("https://api.anthropic.com/v1//", anthropic)
                   == "https://api.anthropic.com/v1/messages");
            expect(RepenteClient::buildUrl("  https://api.anthropic.com  ", anthropic)
                   == "https://api.anthropic.com/v1/messages");
        }

        beginTest("proxy prefixes are preserved");
        {
            expect(RepenteClient::buildUrl("https://proxy.internal/api", anthropic)
                   == "https://proxy.internal/api/v1/messages");
            expect(RepenteClient::buildUrl("https://proxy.internal/api/v1", anthropic)
                   == "https://proxy.internal/api/v1/messages");
        }

        beginTest("base spelling out the full endpoint is left alone");
        {
            expect(RepenteClient::buildUrl("https://api.anthropic.com/v1/messages", anthropic)
                   == "https://api.anthropic.com/v1/messages");
        }
    }
};
static RepenteClientUrlTest repenteClientUrlTest;
