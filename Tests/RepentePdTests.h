#pragma once

#include "RepentePd/UI/PromptBar.h"
#include "RepentePd/Commands/CommandParser.h"

using namespace RepentePd;

class PromptBarSmokeTest : public UnitTest {
public:
    PromptBarSmokeTest() : UnitTest("PromptBar smoke test", "RepentePd") {}

    void runTest() override
    {
        beginTest("PromptBar can be instantiated");
        PromptBar bar;
        expect(true, "PromptBar constructed without crash");

        beginTest("PromptBar input is accessible");
        bar.setSize(400, 36);
        expect(true, "PromptBar resized without crash");
    }
};

static PromptBarSmokeTest promptBarSmokeTest;

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
