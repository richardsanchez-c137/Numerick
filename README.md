
# NUMERICK

**A personal AI assistant built from the ground up.**

NUMERICK is an evolving Windows-focused personal assistant
project with a C++ core.




## Current milestone: 0.3.0

- Interactive C++ command-line interface
- Basic command recognition
- Local activity logging
- System status reporting
- Local AI integration through Ollama
- Qwen 3.5 4B model support
- HTTP communication through Windows WinHTTP
- JSON request and response handling
- Error handling for AI requests
- separate command parser
- utomated parser tests

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
