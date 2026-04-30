/*
 // Copyright (c) 2026 Carlos Eduardo Batista (Bidu)
 // pd-repente — see LICENSE.txt
*/

#include "CommandParser.h"

namespace RepentePd {

CommandResult CommandParser::parse(juce::String const& input)
{
    juce::String trimmed = input.trim();

    CommandResult result;
    result.raw = trimmed;

    if (trimmed.isEmpty())
        return result; // UNKNOWN, empty

    if (trimmed.startsWithIgnoreCase("/pds"))
    {
        juce::String remainder = trimmed.substring(4).trim();
        juce::StringArray tokens = juce::StringArray::fromTokens(remainder, " ", "\"");
        tokens.removeEmptyStrings();
        auto r = parsePds(tokens);
        r.raw = trimmed;
        return r;
    }

    if (trimmed.equalsIgnoreCase("/help"))
    {
        result.type = CommandType::HELP;
        return result;
    }

    if (trimmed.equalsIgnoreCase("/clear"))
    {
        result.type = CommandType::CLEAR;
        return result;
    }

    if (trimmed.startsWith("/"))
    {
        result.type  = CommandType::UNKNOWN;
        result.error = "unknown command: " + trimmed.upToFirstOccurrenceOf(" ", false, false);
        return result;
    }

    // No leading slash → natural language passthrough for Phase 3
    result.type = CommandType::PASSTHROUGH;
    return result;
}

CommandResult CommandParser::parsePds(juce::StringArray const& tokens)
{
    CommandResult result;

    if (tokens.isEmpty())
    {
        result.type  = CommandType::UNKNOWN;
        result.error = "missing pd-script command verb";
        return result;
    }

    juce::String verb = tokens[0].toLowerCase();

    if      (verb == "create")  result.type = CommandType::PDS_CREATE;
    else if (verb == "connect") result.type = CommandType::PDS_CONNECT;
    else if (verb == "delete")  result.type = CommandType::PDS_DELETE;
    else if (verb == "list")    result.type = CommandType::PDS_LIST;
    else if (verb == "move")    result.type = CommandType::PDS_MOVE;
    else if (verb == "lua")     result.type = CommandType::PDS_LUA;
    else
    {
        result.type  = CommandType::UNKNOWN;
        result.error = "unknown pd-script command: " + tokens[0];
        return result;
    }

    for (int i = 1; i < tokens.size(); ++i)
        result.args.add(tokens[i]);

    return result;
}

} // namespace RepentePd
