# NUMERICK

**A personal AI assistant built from the ground up in C++.**

NUMERICK is a Windows-focused command-line assistant using a local Ollama model. This project emphasizes small, testable modules, controlled tool execution, and transparent behavior.

## Current development status — 0.5.0

Implemented:
- C++17 command-line assistant
- Local Qwen 3.5 4B integration through Ollama
- `ask` and `agent` commands with bounded in-memory conversation history
- Tool registry and agent orchestration with tool-call/round limits and diagnostics
- Read-only `get_local_time` built-in tool
- Explicit persistent facts stored in a local JSON file
- `remember`, `memories`, and `forget` commands
- Automated tests built and run through CMake and CTest

## Commands

```text
help
hello
status
time
ask Explain an API in one sentence.
agent What time is it right now?
remember My project ID is MINT-482
memories
forget 1
clear
exit
```

`ask` sends a question to the model with recent conversation context. `agent` also supplies registered tool definitions and can execute approved tool requests through the registry.

### Persistent memory controls

Persistent memory is **opt-in**. NUMERICK does not automatically turn every conversation into permanent memory.

- `remember <fact>` explicitly saves one fact and prints its memory ID.
- `memories` lists saved facts and IDs.
- `forget <ID>` deletes the selected saved fact.

Facts are stored in `data/memory.json`. The project `.gitignore` excludes `data/`, so the memory file and activity log should remain local and out of version control. Saved facts are included in subsequent model system prompts when relevant context is available. The prompt context is limited to the newest 20 facts and 4,000 bytes; the store supports up to 50 facts, each up to 500 characters. Exact duplicate facts reuse the existing ID.

Persistent facts survive application restarts. Ordinary conversation history remains session-only and retains up to 10 completed turns. These are separate storage mechanisms.

Only save information you are comfortable sending to the configured local Ollama model in prompts. NUMERICK writes memory command classifications and record IDs to its activity log, not the full fact text.

## Build and test

Requirements:
- CMake 3.20 or newer
- A C++17 compiler (the development setup uses MSYS2 UCRT64 G++)
- Ollama with `qwen3.5:4b` for live model testing

Configure and build from the repository root:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The automated test suite does not require a live Ollama service. Live AI checks do require Ollama and the configured model to be available.

Run NUMERICK:

```powershell
.\build\numerick.exe
```

## Current limitations

- Persistent memory is explicit, user-managed fact storage; the model does not write memory automatically.
- Persistent memory is stored as a local JSON file, not encrypted.
- Memory context uses a character limit rather than a tokenizer-aware token budget.
- The schema validator supports only a subset of JSON Schema.
- Only the local-time built-in tool is registered; arbitrary shell commands and computer control are not enabled.
- Voice, camera understanding, web research, and a graphical interface are not implemented.

## Project principles

- Verify behavior rather than assuming success.
- Distinguish automated tests from manual end-to-end checks.
- Save long-term memory only with explicit user intent.
- Keep user facts, credentials, and runtime data out of version control.
- Use explicit permissions before adding consequential operating-system actions.
