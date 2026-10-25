
# NUMERICK

**A personal AI assistant built from the ground up.**

NUMERICK is an evolving Windows-focused personal assistant
project with a C++ core.





## Current Development Status — 0.4.0

Implemented:
- C++ command-line assistant
- Local Qwen 3.5 4B integration through Ollama
- Command parser with automated tests
- C++ tool registry with automated tests
- Built-in `get_local_time` tool
- AI-driven tool orchestration through the `agent` command
- Tool-round and tool-call limits
- CMake build and CTest integration

Example commands:

```text
hello
status
ask Explain an API in one sentence.
agent What time is it right now?
exit
```

The `ask` command sends a question to the model without registered tools.

The `agent` command provides registered tool definitions and can execute tool requests through the registry. The current built-in tool set is limited to local time.

Not yet implemented:
- Automated tests for the complete tool-agent loop
- Full JSON Schema validation
- General user approval workflows
- Natural voice conversations
- Camera and image understanding
- Persistent conversational memory
- General Windows automation
- Dedicated graphical frontend


## Current AI setup

Install Ollama and download the model:

```powershell
ollama pull qwen3.5:4b
```

Ensure Ollama is running, then compile NUMERICK from the project root:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp src/core/ai_client.cpp -o numerick.exe -lwinhttp
```

Run the application:

```powershell
.\numerick.exe
```

Ask a question:

```text
ask Explain what an API is using a restaurant analogy.
```

NUMERICK currently sends each question to the local Ollama service.
The conversation history is not yet retained by the AI client.

## Still planned

- Natural voice conversations
- Camera and visual understanding
- Persistent conversational memory
- Web research tools
- Controlled Windows automation
- Git workflow assistance
- Automated tests and stronger reliability controls
- Dedicated graphical frontend


## Planned capabilities

- Natural voice conversations
- Camera and visual understanding
- Persistent memory
- Web research
- Controlled Windows automation
- Git and development workflow assistance
- Verification and safety controls

## Requirements

- G++ with C++17 support
- CMake 3.20 or newer (optional for direct compilation)

## Build with G++

From the project root:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic src/main.cpp -o numerick.exe
.\numerick.exe
```

## Build with CMake

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
.\build\numerick.exe
```

## Project principles

- Verify actions rather than assuming success.
- Treat uncertain information honestly.
- Use explicit permissions for consequential actions.
- Keep secrets and personal data out of version control.

## Status

Early development. AI, voice, vision, and automation
integrations have not yet been implemented.


## Automated tests

NUMERICK uses CMake and CTest to build and run its automated tests.

Configure the project:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

Build all targets:

```powershell
cmake --build build
```

Run the tests:

```powershell
ctest --test-dir build --output-on-failure
```

Current test suites:

- `command_parser_tests`: checks command recognition and input handling.
- `tool_registry_tests`: checks registration, lookup, execution, and failure handling.

## Tool registry status

The initial tool registry is implemented and tested independently.

It is not yet connected to AI-generated tool calls, and it does not currently perform real computer operations.


## Automated Testing

NUMERICK uses CMake and CTest to build and run its automated tests.

### Configure the project

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

### Build all targets

```powershell
cmake --build build
```

### Run the test suite

```powershell
ctest --test-dir build --output-on-failure
```

### Current test suites

- **command_parser_tests** — Tests command recognition, aliases, capitalization, whitespace, AI prompt preservation, empty input, and unknown commands.
- **tool_registry_tests** — Tests tool registration, lookup, definition listing, handler execution, invalid arguments, duplicate registrations, and exception handling.

Both test suites have passed in the development environment.

## Tool Registry

NUMERICK includes an initial C++ tool registry that stores tool definitions and their associated handlers.

Each registered tool has a name, description, JSON parameter schema, and C++ handler.

The registry currently supports:

- Registering tools and retrieving their definitions.
- Checking whether a tool is registered.
- Executing a registered handler.
- Rejecting unknown tool names and non-object arguments.
- Returning handler errors as failed results.

**Current limitation:** The registry is not yet connected to AI-generated tool calls and does not currently perform real computer operations. Full JSON Schema validation and a dedicated permission system remain future work.

## Current Development Status

- C++ command-line assistant: Implemented
- Local Qwen AI integration through Ollama: Implemented
- Command parser: Implemented and tested
- Initial tool registry: Implemented and tested independently
- AI-driven tool execution: Planned
- Persistent conversational memory: Planned
- Voice interaction: Planned
- Camera and visual understanding: Planned
- Controlled Windows automation: Planned
- Graphical frontend: Planned


### Tool Agent Tests

The tool-agent test suite uses a fake chat client, allowing the orchestration logic to be tested without running Ollama.

It covers:

- Successful tool-call execution and result delivery.
- Unknown tool requests.
- Malformed tool arguments.
- Tool-round and tool-call limits.
- Empty prompts.

Run the complete test suite:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

The current test suite contains four targets:

- `command_parser_tests`
- `tool_registry_tests`
- `builtin_tools_tests`
- `tool_agent_tests`

Passing automated tests establish the behaviour of the tested components. They do not, by themselves, prove the reliability of every live AI interaction.


### Tool Argument Validation

The registry validates tool definitions when they are registered and checks arguments before invoking a handler.

The current validator supports:

- Object parameter schemas.
- Required properties.
- Basic JSON property types.
- Rejection of unexpected arguments when prohibited by the schema.
- Rejection of malformed schema definitions.

This is a limited subset of JSON Schema. It does not currently implement nested validation or every standard schema constraint.

Automated tests cover invalid definitions, missing arguments, mismatched types, unexpected arguments, and handler execution failures.


### Tool Safety and Diagnostics

The tool registry validates supported schema definitions at registration time and validates tool arguments before invoking handlers.

The current validator supports required properties, basic JSON types, and the `additionalProperties` boolean. It does not implement every JSON Schema feature.

The tool agent also produces diagnostic reports containing model-request counts, tool-call outcomes, and execution-limit stop reasons. The activity log records metadata without storing full AI prompts or tool arguments.

Automated tests cover successful tool execution, unknown tools, malformed requests, schema-validation failures, handler failures, empty prompts, and execution limits.

The local Qwen integration has successfully completed an initial end-to-end `get_local_time` tool call. This is an initial integration test, not proof that every tool interaction is reliable.

- **Agent diagnostics:** Tracks model requests, tool rounds, tool-call outcomes, and stop reasons. The agent can answer directly without invoking unnecessary tools.