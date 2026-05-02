/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once
#include <juce_core/juce_core.h>

class PluginEditor;

namespace RepentePd {

// Parses a pd-file patch string and creates all objects + connections on the
// current canvas via the Executor. Called instead of openPatch() when merge
// mode is active. Must be called from the message thread.
class PatchMerger {
public:
    static void merge(juce::String const& patchContent, PluginEditor* editor);
};

} // namespace RepentePd
