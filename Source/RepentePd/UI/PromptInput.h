/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#pragma once

#include "Sidebar/CommandInput.h"
#include "RepentePd/Core/Executor.h"

struct lua_State;

namespace RepentePd { class Bridge; }

namespace RepentePd {

// Unified prompt bar: routes /pds to Executor, slash-prefixed commands to
// CommandInput base, and free text to the Repente LLM bridge (Phase 03).
class PromptInput final : public CommandInput {
public:
    PromptInput(PluginEditor* editor, Executor* executor);
    ~PromptInput() override = default;

    SmallArray<std::pair<int, String>> executeCommand(pd::Instance* pd, String message) override;

    // Called after every async REPL callback. PluginEditor sets this to refresh ObjectTreePanel.
    std::function<void()> onRegistryChanged;

    void setBridge(Bridge* b) { bridge = b; }

protected:
    StringArray const& getHelperCommands() const override { return pdsHelperCommands; }
    StringArray const& getObjectHelperCommands() const override { return pdsHelperCommands; }

private:
    void registerPdsTable(lua_State* L);

    static int lua_pds_create (lua_State* L);
    static int lua_pds_connect(lua_State* L);
    static int lua_pds_delete (lua_State* L);
    static int lua_pds_move   (lua_State* L);
    static int lua_pds_list   (lua_State* L);

    PluginEditor* pluginEditor;
    Executor*     executor;
    Bridge*       bridge = nullptr;

    static inline StringArray const pdsHelperCommands = {
        "/pds create", "/pds connect", "/pds delete", "/pds list", "/pds move"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PromptInput)
};

} // namespace RepentePd
