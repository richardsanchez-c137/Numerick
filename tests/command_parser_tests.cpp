
#include "numerick/command_parser.hpp"

#include <cassert>
#include <iostream>
#include <string>

using numerick::CommandType;
using numerick::parseCommand;

int main() {
    // Basic commands
    assert(parseCommand("hello").type == CommandType::Greeting);
    assert(parseCommand("hi").type == CommandType::Greeting);
    assert(parseCommand("hey").type == CommandType::Greeting);

    // Capitalization and whitespace
    assert(parseCommand("  HeY  ").type == CommandType::Greeting);
    assert(parseCommand("\tSTATUS\n").type == CommandType::Status);

    // Command aliases
    assert(parseCommand("commands").type == CommandType::Help);
    assert(parseCommand("quit").type == CommandType::Exit);

    // Other supported commands
    assert(parseCommand("help").type == CommandType::Help);
    assert(parseCommand("status").type == CommandType::Status);
    assert(parseCommand("time").type == CommandType::Time);
    assert(parseCommand("clear").type == CommandType::Clear);
    assert(parseCommand("exit").type == CommandType::Exit);

    // AI questions preserve original capitalization
    const auto question = parseCommand(
        "ask Explain APIs using C++"
    );

    assert(question.type == CommandType::Ask);
    assert(question.argument == "Explain APIs using C++");

    // Extra whitespace after the command
    const auto spacedQuestion = parseCommand(
        "  ask    How Does HTTP Work?  "
    );

    assert(spacedQuestion.type == CommandType::Ask);
    assert(spacedQuestion.argument == "How Does HTTP Work?");

    // Empty question
    const auto emptyQuestion = parseCommand("ask");

    assert(emptyQuestion.type == CommandType::Ask);
    assert(emptyQuestion.argument.empty());

    // Empty and unsupported input
    assert(parseCommand("").type == CommandType::Unknown);
    assert(parseCommand("   ").type == CommandType::Unknown);
    assert(parseCommand("launch browser").type == CommandType::Unknown);

    // Agent command recognition
    const auto agentRequest = parseCommand(
        "agent What time is it right now?"
    );

    assert(agentRequest.type == CommandType::Agent);
    assert(agentRequest.argument == "What time is it right now?");

    // Case-insensitive command recognition
    const auto uppercaseAgent = parseCommand(
        "AGENT Explain this request"
    );

    assert(uppercaseAgent.type == CommandType::Agent);
    assert(uppercaseAgent.argument == "Explain this request");

    // Empty agent request
    const auto emptyAgent = parseCommand("agent");

    assert(emptyAgent.type == CommandType::Agent);
    assert(emptyAgent.argument.empty());

    std::cout << "All command parser tests passed.\n";

    return 0;
}
