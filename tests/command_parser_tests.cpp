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
    assert(parseCommand("doctor").type == CommandType::Doctor);
    assert(parseCommand(" DOCTOR ").type == CommandType::Doctor);
    assert(parseCommand("time").type == CommandType::Time);
    assert(parseCommand("voice").type == CommandType::Voice);
    assert(parseCommand("VOICE").type == CommandType::Voice);
    assert(parseCommand("clear").type == CommandType::Clear);
    assert(parseCommand("exit").type == CommandType::Exit);

    // AI questions preserve original capitalization
    const auto question = parseCommand("ask Explain APIs using C++");
    assert(question.type == CommandType::Ask);
    assert(question.argument == "Explain APIs using C++");

    const auto spacedQuestion = parseCommand("  ask    How Does HTTP Work?  ");
    assert(spacedQuestion.type == CommandType::Ask);
    assert(spacedQuestion.argument == "How Does HTTP Work?");

    const auto emptyQuestion = parseCommand("ask");
    assert(emptyQuestion.type == CommandType::Ask);
    assert(emptyQuestion.argument.empty());

    assert(parseCommand("").type == CommandType::Unknown);
    assert(parseCommand("   ").type == CommandType::Unknown);
    assert(parseCommand("launch browser").type == CommandType::Unknown);

    // Agent command recognition
    const auto agentRequest = parseCommand("agent What time is it right now?");
    assert(agentRequest.type == CommandType::Agent);
    assert(agentRequest.argument == "What time is it right now?");

    const auto uppercaseAgent = parseCommand("AGENT Explain this request");
    assert(uppercaseAgent.type == CommandType::Agent);
    assert(uppercaseAgent.argument == "Explain this request");

    const auto emptyAgent = parseCommand("agent");
    assert(emptyAgent.type == CommandType::Agent);
    assert(emptyAgent.argument.empty());

    // Explicit persistent-memory management commands
    const auto remember = parseCommand(
        "remember My project ID is MINT-482"
    );
    assert(remember.type == CommandType::Remember);
    assert(remember.argument == "My project ID is MINT-482");

    const auto emptyRemember = parseCommand("remember");
    assert(emptyRemember.type == CommandType::Remember);
    assert(emptyRemember.argument.empty());

    assert(parseCommand("memories").type == CommandType::Memories);
    assert(parseCommand("MEMORY").type == CommandType::Memories);

    const auto forget = parseCommand("forget 12");
    assert(forget.type == CommandType::Forget);
    assert(forget.argument == "12");

    const auto emptyForget = parseCommand("forget");
    assert(emptyForget.type == CommandType::Forget);
    assert(emptyForget.argument.empty());

    std::cout << "All command parser tests passed.\n";
    return 0;
}
