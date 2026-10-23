
# NUMERICK

**A personal AI assistant built from the ground up.**

NUMERICK is an evolving Windows-focused personal assistant
project with a C++ core.

## Current milestone: 0.1.0

- Interactive command-line interface
- Basic command recognition
- Local activity logging
- System status reporting
- C++17 foundation

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
