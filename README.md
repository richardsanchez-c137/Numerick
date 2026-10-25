# NUMERICK

**A personal AI assistant built from the ground up in C++.**

NUMERICK is a Windows-focused command-line assistant using a local Ollama model. Its core emphasizes small, testable modules, bounded conversation context, user-managed persistent memory, and read-only tools with explicit access boundaries.

## Current development status — 0.9.0

Implemented:
- C++17 command-line assistant
- Local Qwen 3.5 4B integration through Ollama
- `ask` and `agent` commands with bounded in-memory conversation history
- Tool registry and agent orchestration with tool-call/round limits and diagnostics
- Read-only `get_local_time` tool
- Explicit persistent facts stored locally in `data/memory.json`
- `remember`, `memories`, and `forget` commands
- Read-only workspace tools: `list_project_files`, `read_project_file`, `search_project_files`, `find_cpp_symbol`, `describe_project_architecture`, `inspect_cpp_component`, and `inspect_cpp_member_relationship`
- Automated tests through CMake and CTest

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

`ask` sends a question to the model with recent conversation context. `agent` also supplies registered tool definitions and can execute tools from the approved registry.

## Persistent memory

Persistent memory is **opt-in**. NUMERICK does not automatically turn every conversation into permanent memory.

- `remember <fact>` explicitly saves a fact and prints its memory ID.
- `memories` lists saved facts and IDs.
- `forget <ID>` deletes the selected fact.

Facts are stored in `data/memory.json`; `data/` is ignored by Git. Saved facts may be included in the system prompt. The prompt context is limited to the newest 20 facts and 4,000 bytes; the store supports up to 50 facts, each up to 500 characters. Exact duplicates reuse the existing ID.

Saved facts survive application restarts. Ordinary conversation history remains session-only and retains up to 10 completed turns. These are separate systems. Memory is local plaintext, not encrypted. Save only information you are comfortable sending to the configured local Ollama model in prompts.

## Read-only workspace and codebase tools

The `agent` command can use these registered tools when a request requires project-file inspection:

- `list_project_files`: lists accessible entries in a directory relative to the workspace root. Use `.` for the root.
- `read_project_file`: reads one allowlisted text file by workspace-relative path.
- `search_project_files`: searches allowlisted workspace text files for a literal, case-insensitive query and returns path/line/snippet matches. Scans are bounded.
- `find_cpp_symbol`: searches C/C++ source and header files for whole-identifier occurrences and reports line numbers. This is bounded text search, not a compiler-grade parser.
- `describe_project_architecture`: builds a bounded, read-only inventory of workspace directories. It reports filesystem structure rather than a complete call graph.
- `inspect_cpp_component`: groups bounded C++ identifier matches into header/interface candidates, likely implementations, likely call sites, related test references, and other source references. Categories are heuristics, not compiler-resolved relationships.
- `inspect_cpp_member_relationship`: compares two named class members, reports source-backed value/reference/pointer syntax and likely initializer wiring, and clearly identifies incomplete evidence. It is heuristic, not a compiler or lifetime analyzer.

The default workspace root is the process's current working directory. Start NUMERICK from the repository root to give it access to that project. To select a different root, set `NUMERICK_WORKSPACE_ROOT` before launching the application:

```powershell
$env:NUMERICK_WORKSPACE_ROOT = "D:\GitHub\Projects\SomeOtherProject"
.\build\numerick.exe
```

The tools are read-only: there are no file-writing, deletion, process-execution, or arbitrary shell tools. They reject absolute paths and `..` traversal, verify resolved paths stay under the canonical workspace root, filter private directories such as `.git`, `build`, `data`, and `logs`, block common secret filenames and key/database/binary extensions, allowlist text file types, cap reads at 256 KiB, and cap returned content at 12 KiB. Files returned by these tools are marked as untrusted data in their tool response and the agent system prompt instructs the model not to follow instructions inside files.

Search, symbol lookup, architecture inventory, component inspection, and member-relationship checks are bounded and read-only. Component inspection reuses the existing C++ identifier search and groups results with path/snippet heuristics; it does not resolve overloads, templates, aliases, or semantic call graphs, and bounded search may omit matches in very large workspaces.

Agent runs also print a compact diagnostic summary with model-request counts, tool-round counts, and per-tool outcomes. Rejected or failed calls are reported separately from successful calls.

These filters reduce accidental exposure but are not a complete operating-system sandbox. Run NUMERICK as a normal user account, and choose the workspace root deliberately. Do not point it at your entire home directory or a folder containing unrelated private files.

## Build and test

Requirements:
- CMake 3.20 or newer
- A C++17 compiler (the development setup uses MSYS2 UCRT64 G++)
- Ollama with `qwen3.5:4b` for live model testing

Configure, build, and run all automated tests from the repository root:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Automated tests do not require a live Ollama service. Live AI tests require Ollama and the configured model.

Run NUMERICK:

```powershell
.\build\numerick.exe
```

## Current limitations

- Persistent memory is explicit, user-managed fact storage; the model does not write permanent facts automatically.
- Persistent memory is not encrypted.
- Memory context uses a character limit rather than a tokenizer-aware token budget.
- JSON Schema validation supports only a subset of the standard.
- Project file tools are read-only and intentionally exclude many paths and file types; they are not a substitute for a full security sandbox.
- Only local-time and scoped, read-only workspace/codebase tools are registered. Arbitrary shell commands and computer control are not enabled.
- Voice, camera understanding, web research, and a graphical interface are not implemented.

## Project principles

- Verify behavior rather than assuming success.
- Distinguish automated tests from manual end-to-end checks.
- Save long-term memory only with explicit user intent.
- Keep user facts, credentials, and runtime data out of version control.
- Use explicit permissions before adding consequential operating-system actions.
