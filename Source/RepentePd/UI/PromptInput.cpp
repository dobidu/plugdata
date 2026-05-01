/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "Utility/Config.h"
#include "PromptInput.h"
#include "RepentePd/Commands/CommandParser.h"
#include "RepentePd/Commands/SugarExpander.h"
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
                "LLM bridge -- Phase 03 (not yet available)\n"
                "Free-text input will send prompts to a Repente/OpenAI-compat server.\n"
                "Configure server URL and model in /config panel (Phase 03).");
        } else if (topic == "commands") {
            pdInstance->logMessage(
                "pd-repente commands:\n"
                "  /pds <cmd>     -- pd-script (see /help pds)\n"
                "  /lua <expr>    -- run Lua expression\n"
                "  /help [topic]  -- this help\n"
                "  /clear         -- clear the console\n"
                "  <free text>    -- send to LLM (Phase 03)\n"
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

    // Free text → Repente LLM prompt (bridge wired in Phase 03)
    pdInstance->logMessage("repente: " + msg + " [Phase 03]");
    return {};
}

} // namespace RepentePd
