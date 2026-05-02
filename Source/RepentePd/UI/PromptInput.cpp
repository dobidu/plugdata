/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PromptInput.h"
#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Commands/SugarExpander.h"
#include "RepentePd/Bridge/Bridge.h"
#include "RepentePd/Bridge/RepenteClient.h"
#include "RepentePd/Bridge/CanvasSerializer.h"
#include "Utility/SettingsFile.h"
extern "C" {
#include <pd-lua/luas/luajit/src/lua.h>
#include <pd-lua/luas/luajit/src/lauxlib.h>
}

namespace RepentePd {

// ── Lua C closures for the pds table ─────────────────────────────────────────
// upvalue 1 = PromptInput*  (lightuserdata)
// Static members of PromptInput — have access to private fields via self.

int PromptInput::lua_pds_create(lua_State* L)
{
    auto* self = static_cast<PromptInput*>(lua_touserdata(L, lua_upvalueindex(1)));
    juce::String objText = luaL_checkstring(L, 1);
    int x = (int)luaL_optinteger(L, 2, 100);
    int y = (int)luaL_optinteger(L, 3, 100);
    auto cmd = CommandParser::parse(
        "/pds create " + objText + " " + juce::String(x) + " " + juce::String(y));
    auto result = self->executor->executeSync(cmd);
    if (self->pluginEditor && self->pluginEditor->pd)
        self->pluginEditor->pd->logMessage(result);
    if (self->onRegistryChanged) self->onRegistryChanged();
    lua_pushstring(L, self->executor->getLastCreatedName().toRawUTF8());
    return 1;
}

int PromptInput::lua_pds_connect(lua_State* L)
{
    auto* self = static_cast<PromptInput*>(lua_touserdata(L, lua_upvalueindex(1)));
    juce::String a  = luaL_checkstring(L, 1);
    int nout        = (int)luaL_optinteger(L, 2, 0);
    juce::String b  = luaL_checkstring(L, 3);
    int nin         = (int)luaL_optinteger(L, 4, 0);
    auto cmd = CommandParser::parse(
        "/pds connect " + a + " " + b + " " + juce::String(nout) + " " + juce::String(nin));
    auto result = self->executor->executeSync(cmd);
    if (self->pluginEditor && self->pluginEditor->pd)
        self->pluginEditor->pd->logMessage(result);
    return 0;
}

int PromptInput::lua_pds_delete(lua_State* L)
{
    auto* self = static_cast<PromptInput*>(lua_touserdata(L, lua_upvalueindex(1)));
    juce::String name = luaL_checkstring(L, 1);
    auto cmd = CommandParser::parse("/pds delete " + name);
    auto result = self->executor->executeSync(cmd);
    if (self->pluginEditor && self->pluginEditor->pd)
        self->pluginEditor->pd->logMessage(result);
    if (self->onRegistryChanged) self->onRegistryChanged();
    return 0;
}

int PromptInput::lua_pds_move(lua_State* L)
{
    auto* self = static_cast<PromptInput*>(lua_touserdata(L, lua_upvalueindex(1)));
    juce::String name = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    auto cmd = CommandParser::parse(
        "/pds move " + name + " " + juce::String(x) + " " + juce::String(y));
    auto result = self->executor->executeSync(cmd);
    if (self->pluginEditor && self->pluginEditor->pd)
        self->pluginEditor->pd->logMessage(result);
    return 0;
}

int PromptInput::lua_pds_list(lua_State* L)
{
    auto* self = static_cast<PromptInput*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto cmd = CommandParser::parse("/pds list");
    auto result = self->executor->executeSync(cmd);
    if (self->pluginEditor && self->pluginEditor->pd)
        self->pluginEditor->pd->logMessage(result);
    return 0;
}

void PromptInput::registerPdsTable(lua_State* L)
{
    lua_newtable(L);

    struct { const char* name; lua_CFunction fn; } fns[] = {
        { "create",  lua_pds_create  },
        { "connect", lua_pds_connect },
        { "delete",  lua_pds_delete  },
        { "move",    lua_pds_move    },
        { "list",    lua_pds_list    },
    };
    for (auto const& f : fns) {
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, f.fn, 1);
        lua_setfield(L, -2, f.name);
    }

    lua_setglobal(L, "pds");
}

// ─────────────────────────────────────────────────────────────────────────────

PromptInput::PromptInput(PluginEditor* ed, Executor* ex)
    : CommandInput(ed)
    , pluginEditor(ed)
    , executor(ex)
{
    registerLuaExtension([this](lua_State* L) { registerPdsTable(L); });

    mergeToggle.setClickingTogglesState(true);
    bool const storedMerge = SettingsFile::getInstance()->getProperty<bool>("repente_merge_mode");
    mergeToggle.setToggleState(storedMerge, juce::dontSendNotification);
    mergeToggle.setTooltip("Merge LLM patch into current canvas (no new tab)");
    mergeToggle.onClick = [this] {
        bool const on = mergeToggle.getToggleState();
        if (bridge) bridge->setMergeMode(on);
        SettingsFile::getInstance()->setProperty("repente_merge_mode", on);
        SettingsFile::getInstance()->saveSettings();
    };
    addAndMakeVisible(mergeToggle);
}

void PromptInput::setBridge(Bridge* b)
{
    bridge = b;
    if (bridge)
        bridge->setMergeMode(mergeToggle.getToggleState());
}

void PromptInput::resized()
{
    CommandInput::resized();

    constexpr int clearW  = 30;
    constexpr int toggleW = 56;
    constexpr int gap     = 8;
    constexpr int total   = toggleW + gap; // 64px trimmed from commandInput

    for (auto* child : getChildren()) {
        if (dynamic_cast<juce::TextEditor*>(child)) {
            child->setBounds(child->getBounds().withTrimmedRight(total));
            break;
        }
    }

    int const inputY = getHeight() - 30;
    mergeToggle.setBounds(getWidth() - clearW - gap - toggleW, inputY, toggleW, 28);
}

SmallArray<std::pair<int, String>> PromptInput::executeCommand(pd::Instance* pdInstance, String msg)
{
    msg = msg.trim();
    if (msg.isEmpty())
        return {};

    // /help [topic] and /clear handled before all routing
    if (msg.startsWith("/help")) {
        auto topic = msg.substring(5).trim().toLowerCase();
        if (topic.isEmpty()) {
            pdInstance->logMessage(
                "pd-repente REPL -- use /help <topic> for details\n"
                "  topics: pds  sugar  lua  llm  commands  builtin");
        } else if (topic == "pds") {
            pdInstance->logMessage(
                "/pds create <type> [x y] [args] -- create object on canvas\n"
                "/pds connect <a> <out> <b> <in> -- connect two objects\n"
                "/pds delete <name>              -- remove named object\n"
                "/pds move <name> <x> <y>        -- reposition object\n"
                "/pds list                       -- list all REPL objects");
        } else if (topic == "sugar") {
            pdInstance->logMessage(
                "Sugar syntax (expands before parsing):\n"
                "  @type [args]    -> /pds create type [args]\n"
                "  ~type [args]    -> /pds create type~ [args]\n"
                "  -> type [args]  -> create + auto-connect from last object\n"
                "  $last           -> expands to last created object name");
        } else if (topic == "lua") {
            pdInstance->logMessage(
                "Lua blocks -- wrap in { }:\n"
                "  { math.random() * 440 }\n"
                "  { pd.post(\"hello\") }\n"
                "Multi-line: open { + Enter, close } to run.\n"
                "\n"
                "pds table (synchronous pd-script from Lua):\n"
                "  local n = pds.create(\"osc~\", 100, 100)\n"
                "  local d = pds.create(\"dac~\", 100, 200)\n"
                "  pds.connect(n, 0, d, 0)\n"
                "  pds.delete(n)\n"
                "  pds.move(n, 200, 100)\n"
                "  pds.list()");
            pdInstance->logMessage(
                "pd table (built-in):\n"
                "  pd.post(msg)     -- log to console\n"
                "  pd.eval(command) -- run any REPL command string\n"
                "\n"
                "Loop example:\n"
                "  for i=1,4 do pds.create(\"osc~\", i*80, 100) end");
        } else if (topic == "llm") {
            pdInstance->logMessage(
                "LLM bridge -- configure with /config, then type free text.\n"
                "\n"
                "  /config               -- show current settings\n"
                "  /config url <url>     -- set server URL (OpenAI-compat)\n"
                "  /config model <name>  -- set model (e.g. gpt-4o, repente-1)\n"
                "  /config key <key>     -- set API key (stored in settings)\n"
                "  /config test          -- ping server for connectivity\n"
                "\n"
                "  /analyze <question>   -- ask LLM about patch (text only, no execution)\n"
                "  <free text>           -- generate patch (pd/Lua/pds executed on arrival)\n"
                "\n"
                "Default URL: http://localhost:7860 (Repente local server)\n"
                "Settings persist across restarts.");
        } else if (topic == "commands") {
            pdInstance->logMessage(
                "pd-repente commands:\n"
                "  /pds <cmd>       -- pd-script (see /help pds)\n"
                "  /lua <expr>      -- run Lua expression\n"
                "  /config          -- LLM server config (see /help llm)\n"
                "  /analyze <q>     -- ask LLM about patch (text response)\n"
                "  /canvas          -- show serialized canvas state (debug)\n"
                "  /help [topic]    -- this help\n"
                "  /clear           -- clear the console\n"
                "  <free text>      -- send to LLM bridge\n"
                "\n"
                "Shorthand sugar: see /help sugar\n"
                "Lua + pds API:   see /help lua\n"
                "Built-in REPL:   see /help builtin");
        } else if (topic == "builtin") {
            pdInstance->logMessage(
                "Built-in REPL commands (plugdata):\n"
                "  sel <id>       -- select object by name or index\n"
                "  deselect / >   -- deselect all\n"
                "  ls / list      -- list all objects on canvas\n"
                "  find <id>      -- search objects by name\n"
                "  canvas <msg>   -- send message to canvas\n"
                "  pd <msg>       -- send message to pd (e.g. pd dsp 1)\n"
                "  script <file>  -- run Lua script from search path\n"
                "  reset          -- reset Lua interpreter state\n"
                "  man <cmd>      -- show manual for command\n"
                "  { expr }       -- evaluate Lua expression\n"
                "  <id> > <msg>   -- send message to named object");
        } else {
            pdInstance->logMessage("unknown topic: " + topic
                + "\navailable: pds  sugar  lua  llm  commands  builtin");
        }
        return {};
    }
    if (msg == "/clear") {
        pluginEditor->clearConsole();
        return {};
    }

    if (msg == "/canvas") {
        if (auto* canvas = pluginEditor ? pluginEditor->getCurrentCanvas() : nullptr) {
            auto snapshot = RepentePd::CanvasSerializer::serialize(canvas);
            pdInstance->logMessage(snapshot.isNotEmpty() ? snapshot : "(empty canvas)");
        } else {
            pdInstance->logMessage("repente: no active canvas");
        }
        return {};
    }

    if (msg.startsWith("/analyze")) {
        juce::String question = msg.substring(8).trim();
        if (question.isEmpty()) {
            pdInstance->logMessage(
                "usage: /analyze <question>\n"
                "  Sends question + canvas context to LLM.\n"
                "  Response shown as text -- no patch execution.");
            return {};
        }
        if (!bridge) {
            pdInstance->logMessage("repente: bridge not ready -- use /config url to set server");
            return {};
        }
        bridge->send(question, /*analyzeOnly=*/true);
        return {};
    }

    if (msg.startsWith("/config")) {
        auto args = msg.substring(7).trim();

        if (args.isEmpty()) {
            auto const& cfg = bridge ? bridge->getConfig()
                                     : RepentePd::RepenteClient::Config{};
            juce::String masked = cfg.apiKey.isNotEmpty() ? "****" : "(not set)";
            pdInstance->logMessage(
                "repente config:\n"
                "  url:   " + cfg.url + "\n"
                "  model: " + cfg.model + "\n"
                "  key:   " + masked);
            return {};
        }

        if (args.startsWith("url ")) {
            juce::String newUrl = args.substring(4).trim();
            SettingsFile::getInstance()->setProperty("repente_url", newUrl);
            SettingsFile::getInstance()->saveSettings();
            if (bridge) {
                auto cfg = bridge->getConfig();
                cfg.url = newUrl;
                bridge->setConfig(std::move(cfg));
            }
            pdInstance->logMessage("repente: url set to " + newUrl);
            return {};
        }

        if (args.startsWith("model ")) {
            juce::String newModel = args.substring(6).trim();
            SettingsFile::getInstance()->setProperty("repente_model", newModel);
            SettingsFile::getInstance()->saveSettings();
            if (bridge) {
                auto cfg = bridge->getConfig();
                cfg.model = newModel;
                bridge->setConfig(std::move(cfg));
            }
            pdInstance->logMessage("repente: model set to " + newModel);
            return {};
        }

        if (args.startsWith("key ")) {
            juce::String newKey = args.substring(4).trim();
            SettingsFile::getInstance()->setProperty("repente_key", newKey);
            SettingsFile::getInstance()->saveSettings();
            if (bridge) {
                auto cfg = bridge->getConfig();
                cfg.apiKey = newKey;
                bridge->setConfig(std::move(cfg));
            }
            pdInstance->logMessage("repente: api key set (masked)");
            return {};
        }

        if (args == "test") {
            if (!bridge) {
                pdInstance->logMessage("repente: bridge not ready");
                return {};
            }
            auto url = bridge->getConfig().url;
            pdInstance->logMessage("repente: testing " + url + "...");
            bridge->ping([pd = pdInstance](bool ok, juce::String const& msg) {
                if (ok)
                    pd->logMessage("repente: connected -- " + msg);
                else
                    pd->logMessage("repente: connection failed -- " + msg);
            });
            return {};
        }

        pdInstance->logMessage(
            "usage:\n"
            "  /config              -- show current config\n"
            "  /config url <url>    -- set server URL\n"
            "  /config model <name> -- set model name\n"
            "  /config key <key>    -- set API key\n"
            "  /config test         -- test server connection");
        return {};
    }

    // Arrow sugar: → or -> → /pds create + auto-connect
    if (SugarExpander::isArrow(msg)) {
        juce::String const preArrowLast = executor->getLastCreatedName();
        auto const expanded            = SugarExpander::expand(msg, preArrowLast);
        auto cmd                       = CommandParser::parse(expanded);
        executor->submit(cmd, [this, ex = executor, preArrowLast, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (onRegistryChanged) onRegistryChanged();
            if (preArrowLast.isNotEmpty()) {
                juce::String const newName = ex->getLastCreatedName();
                if (newName.isNotEmpty() && newName != preArrowLast) {
                    auto connectCmd = CommandParser::parse(
                        "/pds connect " + preArrowLast + " " + newName + " 0 0");
                    ex->submit(connectCmd, [this, pd](juce::String const& m) {
                        pd->logMessage(m);
                        if (onRegistryChanged) onRegistryChanged();
                    });
                }
            }
        });
        return {};
    }

    // /pds DSL → Executor
    if (msg.startsWith("/pds")) {
        juce::String const lastName = executor->getLastCreatedName();
        auto const expanded         = SugarExpander::expand(msg, lastName);
        auto cmd                    = CommandParser::parse(expanded);
        executor->submit(cmd, [this, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (onRegistryChanged) onRegistryChanged();
        });
        return {};
    }

    // /lua <expr> → wrap in {} and delegate to base Lua engine
    if (msg.startsWith("/lua ")) {
        return CommandInput::executeCommand(pdInstance, "{" + msg.substring(5).trim() + "}");
    }

    // Any other /command → strip leading / and delegate to CommandInput
    if (msg.startsWith("/")) {
        return CommandInput::executeCommand(pdInstance, msg.substring(1));
    }

    // {expr}, <id> > <msg>, > (deselect) → delegate to CommandInput
    if (msg.startsWith("{") || msg.contains(" > ") || msg.trimStart().startsWith(">")) {
        return CommandInput::executeCommand(pdInstance, msg);
    }

    // Free text → Repente LLM bridge
    if (bridge) {
        bridge->send(msg);
    } else {
        pdInstance->logMessage("repente: not configured -- use /help llm");
    }
    return {};
}

} // namespace RepentePd
