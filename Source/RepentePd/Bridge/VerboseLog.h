/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>
#include <deque>
#include <utility>

namespace RepentePd {

struct VerboseTurn {
    juce::String request;
    juce::String response;
};

// Runtime-only registry keyed by an incrementing turn id, so the console can
// show a single short line per turn ("repente: verbose #N - <prompt>") while
// the full request/response text stays available on demand (right-clicking
// that line in Source/Sidebar/Console.h looks a turn up here). Capped like
// the console's own message history (Source/Pd/Instance.cpp's 800-message
// cap) so a long session doesn't grow this unbounded.
class VerboseLog {
public:
    static int record(juce::String const& request)
    {
        int const id = nextId++;
        turns.push_back({id, {request, {}}});
        while (turns.size() > maxTurns)
            turns.pop_front();
        return id;
    }

    static void setResponse(int turnId, juce::String const& response)
    {
        for (auto& [id, turn] : turns) {
            if (id == turnId) {
                turn.response = response;
                return;
            }
        }
    }

    // Appends the literal HTTP request (method/URL/headers/body, secrets
    // redacted) to the turn's request text, once RepenteClient has built it.
    static void appendRequestDetail(int turnId, juce::String const& detail)
    {
        for (auto& [id, turn] : turns) {
            if (id == turnId) {
                turn.request += "\n\n" + detail;
                return;
            }
        }
    }

    static bool find(int turnId, VerboseTurn& out)
    {
        for (auto const& [id, turn] : turns) {
            if (id == turnId) {
                out = turn;
                return true;
            }
        }
        return false;
    }

private:
    static constexpr size_t maxTurns = 200;
    static inline int nextId = 0;
    static inline std::deque<std::pair<int, VerboseTurn>> turns;
};

} // namespace RepentePd
