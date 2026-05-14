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
#include "RepentePd/SpectralAnalyzer.h"
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

    // Label is a child of the toggle so clicks on "merge" text propagate to the checkbox.
    mergeLabel.setFont(Fonts::getDefaultFont().withHeight(13.0f));
    mergeLabel.setJustificationType(juce::Justification::centredLeft);
    mergeToggle.addAndMakeVisible(mergeLabel);
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

    // Layout: [☐ merge] [>] [text field ............] [×]
    // mergeToggle = checkbox (22px) + mergeLabel child fills the rest.
    constexpr int leftPad  = 4;
    constexpr int checkW   = 22;
    constexpr int labelW   = 40;
    constexpr int toggleW  = checkW + labelW; // 62px total
    constexpr int gap      = 4;

    for (auto* child : getChildren()) {
        if (dynamic_cast<juce::TextEditor*>(child)) {
            int const newLeft = leftPad + toggleW + gap + consoleTargetLength + gap;
            child->setBounds(child->getBounds().withLeft(newLeft));
            break;
        }
    }

    mergeToggle.setBounds(leftPad, getHeight() - 30, toggleW, 28);
    // Label positioned inside toggle's local bounds, right of checkbox area
    mergeLabel.setBounds(checkW, 0, labelW, 28);
}

void PromptInput::paintOverChildren(Graphics& g)
{
    // Draw ">" to the right of the merge toggle, suppressing the default left-edge position.
    g.setColour(PlugDataColours::sidebarTextColour);
    g.setFont(Fonts::getSemiBoldFont().withHeight(15));
    g.drawText(consoleTargetName,
               mergeToggle.getRight() + 4, mergeToggle.getY(),
               consoleTargetLength, mergeToggle.getHeight(),
               Justification::centredLeft);
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
            pdInstance->logRepente(juce::String::fromUTF8("pd-repente  \xe2\x94\x80\xe2\x94\x80  /help <topic> for details"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  \xe2\x80\xa2 pds       pd-script REPL\n"
                "  \xe2\x80\xa2 sugar     shorthand syntax\n"
                "  \xe2\x80\xa2 lua       Lua scripting\n"
                "  \xe2\x80\xa2 llm       LLM bridge + /analyze\n"
                "  \xe2\x80\xa2 commands  all /commands\n"
                "  \xe2\x80\xa2 builtin   plugdata built-ins"));
        } else if (topic == "pds") {
            pdInstance->logRepente(juce::String::fromUTF8("pd-script REPL  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  /pds create <type> [x y] [args]  \xe2\x86\x92 create object\n"
                "  /pds connect <a> <out> <b> <in>  \xe2\x86\x92 connect objects\n"
                "  /pds delete <name>               \xe2\x86\x92 remove object\n"
                "  /pds move <name> <x> <y>         \xe2\x86\x92 reposition\n"
                "  /pds list                        \xe2\x86\x92 list REPL objects"));
        } else if (topic == "sugar") {
            pdInstance->logRepente(juce::String::fromUTF8("Sugar syntax  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  @type [args]    \xe2\x86\x92 /pds create type [args]\n"
                "  ~type [args]    \xe2\x86\x92 /pds create type~ [args]\n"
                "  -> type [args]  \xe2\x86\x92 create + auto-connect from last object\n"
                "  $last           \xe2\x86\x92 expands to last created object name"));
        } else if (topic == "lua") {
            pdInstance->logRepente(juce::String::fromUTF8("Lua scripting  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  Wrap in { }:  { math.random() * 440 }\n"
                "  Multi-line:   open { + Enter, close } to run\n"
                "\n"
                "  pds.create(\"osc~\", x, y)   \xe2\x86\x92 create + return name\n"
                "  pds.connect(a, nout, b, nin)\n"
                "  pds.delete(name)\n"
                "  pds.move(name, x, y)\n"
                "  pds.list()\n"
                "  pd.post(msg)          \xe2\x86\x92 log to console\n"
                "  pd.eval(cmd)          \xe2\x86\x92 run any REPL command"));
        } else if (topic == "llm") {
            pdInstance->logRepente(juce::String::fromUTF8("LLM bridge  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  /config url <url>           \xe2\x86\x92 set server (OpenAI-compat)\n"
                "  /config model <name>        \xe2\x86\x92 set model\n"
                "  /config key <key>           \xe2\x86\x92 set API key (stored)\n"
                "  /config history on|off      \xe2\x86\x92 persist history across sessions\n"
                "  /config autoplace on|off    \xe2\x86\x92 cursor placement (off = LLM coords)\n"
                "  /config test                \xe2\x86\x92 ping server\n"
                "  /config                     \xe2\x86\x92 show current settings\n"
                "\n"
                "  /analyze <question>  \xe2\x86\x92 ask LLM, text only, no execution\n"
                "  /listen              \xe2\x86\x92 capture 3s audio \xe2\x86\x92 spectral analysis (text)\n"
                "  /listen <prompt>     \xe2\x86\x92 capture 3s audio \xe2\x86\x92 spectral \xe2\x86\x92 generate/modify\n"
                "  /history             \xe2\x86\x92 show conversation turn count\n"
                "  /history clear       \xe2\x86\x92 wipe conversation history\n"
                "  <free text>          \xe2\x86\x92 generate patch (auto-detected + executed)\n"
                "\n"
                "  Backends:\n"
                "    Ollama (local, free):\n"
                "      /config url http://localhost:11434\n"
                "      /config model llama3.2\n"
                "    OpenAI:\n"
                "      /config url https://api.openai.com\n"
                "      /config key sk-...\n"
                "      /config model gpt-4o\n"
                "    repente server:\n"
                "      /config url http://localhost:7860\n"
                "\n"
                "  Settings persist across sessions."));
        } else if (topic == "commands") {
            pdInstance->logRepente(juce::String::fromUTF8("Commands  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(juce::String::fromUTF8(
                "  /pds <cmd>       \xe2\x86\x92 pd-script  (/help pds)\n"
                "  /lua <expr>      \xe2\x86\x92 Lua inline  (/help lua)\n"
                "  /arrange [dir] [step] \xe2\x86\x92 arrange by signal flow (dir: left-right|right-left|top-down|bottom-up; step=px spacing, default 130)\n"
                "  /analyze <q>     \xe2\x86\x92 ask LLM, no execution\n"
                "  /listen [prompt] \xe2\x86\x92 audio capture \xe2\x86\x92 spectral (no prompt=analyze)\n"
                "  /history         \xe2\x86\x92 show turn count\n"
                "  /history clear   \xe2\x86\x92 wipe conversation history\n"
                "  /config \xe2\x80\xa6        \xe2\x86\x92 LLM config  (/help llm)\n"
                "  /canvas          \xe2\x86\x92 print canvas state (debug)\n"
                "  /help [topic]    \xe2\x86\x92 this help\n"
                "  /clear           \xe2\x86\x92 clear console\n"
                "  <free text>      \xe2\x86\x92 send to LLM bridge"));
        } else if (topic == "builtin") {
            pdInstance->logRepente(juce::String::fromUTF8("Built-in REPL  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"));
            pdInstance->logMessage(
                "  sel <id>        select object\n"
                "  deselect / >    deselect all\n"
                "  ls / list       list all canvas objects\n"
                "  find <id>       search by name\n"
                "  canvas <msg>    send message to canvas\n"
                "  pd <msg>        send message to pd  (pd dsp 1)\n"
                "  script <file>   run Lua script\n"
                "  reset           reset Lua state\n"
                "  man <cmd>       command manual\n"
                "  { expr }        evaluate Lua\n"
                "  <id> > <msg>    send message to named object");
        } else {
            pdInstance->logMessage(juce::String::fromUTF8("unknown topic: ") + topic
                + juce::String::fromUTF8("\n  \xe2\x80\xa2 pds  \xe2\x80\xa2 sugar  \xe2\x80\xa2 lua  \xe2\x80\xa2 llm  \xe2\x80\xa2 commands  \xe2\x80\xa2 builtin"));
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

    if (msg.startsWith("/listen")) {
        if (!bridge) {
            pdInstance->logRepente("repente: bridge not ready -- use /config url to set server");
            return {};
        }
        juce::String prompt = msg.substring(7).trim();
        bool const analyzeOnly = prompt.isEmpty();
        if (analyzeOnly)
            prompt = "I just heard the audio output. Describe the spectral content and suggest how to improve it.";

        if (!pluginEditor || !pluginEditor->pd) return {};
        pluginEditor->pd->startAudioCapture(3.0f);
        pdInstance->logRepente("repente: listening (3s)...");

        listenCancelToken = std::make_shared<std::atomic<bool>>(false);
        auto token = listenCancelToken;
        auto* br  = bridge;
        auto* ppd = pluginEditor->pd;

        juce::Timer::callAfterDelay(3200, [token, br, ppd, prompt, analyzeOnly] {
            if (*token) return;
            auto buffer   = ppd->audioCapture.takeCapture();
            int const sr  = static_cast<int>(ppd->getSampleRate());
            auto result   = RepentePd::SpectralAnalyzer::analyze(buffer, sr > 0 ? sr : 44100);
            auto spectral = RepentePd::SpectralAnalyzer::format(result);
            ppd->logRepente("repente: analyzing audio...");
            br->send(prompt, analyzeOnly, {}, spectral);
        });
        return {};
    }

    if (msg.startsWith("/history")) {
        auto args = msg.substring(8).trim();
        if (args == "clear") {
            if (bridge) bridge->clearHistory();
            pdInstance->logRepente("repente: history cleared");
            return {};
        }
        int turns = bridge ? bridge->historyTurnCount() : 0;
        if (turns == 0)
            pdInstance->logRepente("repente: no conversation history");
        else
            pdInstance->logRepente("repente: " + juce::String(turns)
                + " turn" + (turns == 1 ? "" : "s") + " in history");
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
            pdInstance->logRepente("repente: bridge not ready -- use /config url to set server");
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
            bool const persist    = SettingsFile::getInstance()->getProperty<bool>("repente_persist_history");
            bool const autoplace  = SettingsFile::getInstance()->getProperty<bool>("repente_autoplace");
            // URL backend hint
            juce::String urlNote;
            if (cfg.url.contains("11434"))
                urlNote = "  (Ollama)";
            else if (cfg.url.contains("7860"))
                urlNote = "  (repente server)";
            else if (!cfg.url.startsWithIgnoreCase("http://localhost")
                  && !cfg.url.startsWithIgnoreCase("http://127.0.0.1"))
                urlNote = juce::String::fromUTF8("  (\xe2\x9a\xa0 remote)");
            pdInstance->logRepente("repente config");
            pdInstance->logMessage(
                "  url:       " + cfg.url + urlNote + "\n"
                "  model:     " + cfg.model + "\n"
                "  key:       " + masked + "\n"
                "  history:   " + juce::String(persist ? "persist" : "session-only (default)") + "\n"
                "  autoplace: " + juce::String(autoplace ? "on (default)" : "off"));
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
            pdInstance->logRepente(juce::String::fromUTF8("repente: url \xe2\x86\x92 ") + newUrl);
            // Privacy warning for non-localhost URLs
            auto const urlLower = newUrl.trim().toLowerCase();
            bool const isLocal = urlLower.startsWith("http://localhost")
                              || urlLower.startsWith("http://127.0.0.1")
                              || urlLower.startsWith("https://localhost")
                              || urlLower.startsWith("https://127.0.0.1");
            if (!isLocal)
                pdInstance->logMessage(juce::String::fromUTF8(
                    "  \xe2\x9a\xa0  remote URL: canvas patch data will be sent to this server"));
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
            pdInstance->logRepente(juce::String::fromUTF8("repente: model \xe2\x86\x92 ") + newModel);
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
            pdInstance->logRepente("repente: api key set (masked)");
            return {};
        }

        if (args == "test") {
            if (!bridge) {
                pdInstance->logRepente("repente: bridge not ready");
                return {};
            }
            auto url = bridge->getConfig().url;
            pdInstance->logRepente("repente: testing " + url + "...");
            bridge->ping([pd = pdInstance](bool ok, juce::String const& msg) {
                if (ok)
                    pd->logRepente(juce::String::fromUTF8("repente: connected \xe2\x80\x94 ") + msg);
                else
                    pd->logError(juce::String::fromUTF8("repente: connection failed \xe2\x80\x94 ") + msg);
            });
            return {};
        }

        if (args == "history on" || args == "history off") {
            bool const on = (args == "history on");
            SettingsFile::getInstance()->setProperty("repente_persist_history", on);
            if (!on)
                SettingsFile::getInstance()->setProperty("repente_history", juce::String(""));
            SettingsFile::getInstance()->saveSettings();
            pdInstance->logRepente(juce::String("repente: history persistence ") + (on ? "on" : "off"));
            return {};
        }

        if (args == "autoplace on" || args == "autoplace off") {
            bool const on = (args == "autoplace on");
            SettingsFile::getInstance()->setProperty("repente_autoplace", on);
            SettingsFile::getInstance()->saveSettings();
            pdInstance->logRepente(juce::String("repente: autoplace ") + (on ? "on" : "off"));
            return {};
        }

        pdInstance->logMessage(juce::String::fromUTF8(
            "usage:\n"
            "  /config                     \xe2\x86\x92 show current config\n"
            "  /config url <url>           \xe2\x86\x92 set server URL\n"
            "  /config model <name>        \xe2\x86\x92 set model name\n"
            "  /config key <key>           \xe2\x86\x92 set API key\n"
            "  /config history on|off      \xe2\x86\x92 persist history across sessions\n"
            "  /config autoplace on|off    \xe2\x86\x92 cursor placement (off = use LLM coords)\n"
            "  /config test                \xe2\x86\x92 test server connection"));
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

    // /arrange → topology-aware layout
    if (msg.startsWith("/arrange")) {
        auto cmd = CommandParser::parse(msg);
        executor->submit(cmd, [this, pd = pdInstance](juce::String const& result) {
            pd->logMessage(result);
            if (onRegistryChanged) onRegistryChanged();
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
    // "<id> > <msg>": only match when lhs is a single token (no spaces) — avoids
    // misrouting pasted prompts that contain " > " as English punctuation.
    {
        auto const gtPos   = msg.indexOf(" > ");
        bool const isObjMsg = gtPos > 0 && !msg.substring(0, gtPos).containsChar(' ');
        bool const isDeselect = msg.trimStart() == ">";
        if (msg.startsWith("{") || isObjMsg || isDeselect) {
            return CommandInput::executeCommand(pdInstance, msg);
        }
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
