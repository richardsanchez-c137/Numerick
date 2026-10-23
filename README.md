
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
