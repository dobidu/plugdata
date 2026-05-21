#pragma once

#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Bridge/PdParser.h"
#include "RepentePd/Bridge/OpenAIProvider.h"
#include "RepentePd/Bridge/AnthropicProvider.h"
#include "RepentePd/Bridge/PresetLoader.h"
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
