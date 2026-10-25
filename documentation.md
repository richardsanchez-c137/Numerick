# NUMERICK — Project Documentation

**Project name:** NUMERICK  
**Current version:** 0.8.0
**Project stage:** Local AI assistant core with bounded memory, read-only codebase tools, architecture inventory, and agent diagnostics
**Primary language:** C++  
**Platform:** Windows  
**Build system:** CMake; C++17; MSYS2 UCRT64 G++ development setup  
**Documentation status:** Living document — update as the project evolves


> **Current implementation snapshot — v0.7.0 (October 2026):** The sections below began as the v0.1.0 project plan; some early descriptions are historical. Current code includes a C++17 command-line assistant, local Ollama chat integration, `ask`/`agent`, bounded session history, explicit persistent facts, a validated tool registry, tool orchestration limits, local-time lookup, and five workspace-scoped read-only tools: `list_project_files`, `read_project_file`, `search_project_files`, `find_cpp_symbol`, and `describe_project_architecture`. Agent runs also print model/tool counts and per-tool outcomes for easier troubleshooting. The workspace tools filter private/build folders and sensitive or non-text files, reject paths outside the configured root, and bound returned output. Automated tests are configured through CMake/CTest; verify the current local result with `ctest --test-dir build --output-on-failure` rather than assuming it passed.


---

## 1. Project Overview

NUMERICK is a personal AI assistant being developed from scratch, with the long-term goal of creating a capable, context-aware assistant that can communicate naturally, understand visual information, remember relevant user preferences, research information, and perform everyday computer tasks.

The inspiration comes from fictional AI assistants such as Peter Parker's assistant in Spider-Man.

NUMERICK is intended to go beyond a conventional chatbot. It should eventually understand a user's intention, determine which tools are necessary, execute authorized actions, verify their outcomes, and communicate the results.

The project is being developed incrementally, beginning with a native C++ core. Voice interaction, computer vision, advanced AI reasoning, persistent memory, and the user interface will be introduced in later stages.

### Core development philosophy

- Build incrementally and verify each milestone.
- Keep the architecture modular and maintainable.
- Prefer existing AI models over training a foundation model from scratch.
- Separate AI reasoning from the tools that perform real actions.
- Verify the outcome of important operations instead of assuming success.
- Handle uncertainty honestly.
- Protect private information, credentials, and user data.
- Maintain documentation alongside the implementation.

---

## 2. Project Objectives

The long-term objectives are divided into the following capabilities.

### 2.1 Natural communication

NUMERICK should eventually support natural spoken conversations, wake-word activation, spoken responses, and interruption handling.

### 2.2 Visual understanding

NUMERICK should be able to use a camera or other permitted image sources to understand what the user is showing it.

Example:

The user says, "NUMERICK, look at this medicine bottle and tell me what it is used for."

The intended workflow is to activate the camera, capture a readable image, extract the label information, verify the medicine and formulation against reliable references, and explain the findings.

Medicine identification must not rely on bottle colour or shape alone. If the identity cannot be established reliably, NUMERICK must state the uncertainty and request clearer information.

### 2.3 AI reasoning

NUMERICK should interpret natural-language instructions, determine the user's intended outcome, select appropriate tools, and handle errors or ambiguous requests.

### 2.4 Persistent memory

NUMERICK should eventually remember relevant preferences, project context, and user-approved information across sessions.

Memory should be explicit, manageable, and stored separately from temporary conversation state.

### 2.5 Computer automation

NUMERICK should eventually assist with everyday laptop tasks, including:

- Opening applications and locating files.
- Reading and editing approved files.
- Running development tools and tests.
- Inspecting Git repositories.
- Preparing code changes, commits, and pushes.
- Searching the web and gathering information.
- Reporting the results of completed tasks.

Actions must be governed by permissions appropriate to their risk.

### 2.6 Frontend

A dedicated graphical interface will be developed after the core architecture has matured.

The initial implementation is console-based to keep attention on functionality rather than visual design.

---

## 3. Development Environment

The initial development environment is:

| Component | Current configuration |
|---|---|
| Operating system | Windows |
| Compiler | G++ 16.2.0 through MSYS2 |
| Language standard | C++17 |
| Source editor | Visual Studio Code |
| Build configuration | CMake |
| Version control | Git |
| Repository hosting | GitHub — repository creation pending |

The project is being developed on a laptop with an NVIDIA RTX 4050 Laptop GPU with 6 GB of VRAM, 16 GB of system RAM, a Ryzen 7 7445HS processor, and a 1 TB SSD.

These resources are sufficient to begin developing the hybrid assistant architecture. Model selection and local inference performance will be evaluated separately when AI integration begins.

---

## 4. Current Project Structure

The current directory structure is:

```text
Numerick/
├── data/
│   └── activity.log
├── docs/
├── include/
├── src/
│   ├── core/
│   ├── memory/
│   ├── tools/
│   └── main.cpp
├── tests/
├── .gitignore
├── CMakeLists.txt
├── documentation.md
├── README.md
└── numerick.exe
```

Some directories are currently empty and are reserved for future modules.

### Directory responsibilities

**`src/`**

Contains the C++ implementation.

**`src/core/`**

Reserved for the assistant's core logic, request handling, and orchestration components.

**`src/tools/`**

Reserved for implementations of explicit, permission-controlled tools.

**`src/memory/`**

Reserved for persistent memory and related storage operations.

**`include/`**

Reserved for public headers and interfaces shared across modules.

**`tests/`**

Reserved for automated tests.

**`docs/`**

Reserved for supplementary technical documentation.

**`data/`**

Contains local runtime data, including the activity log.

**`CMakeLists.txt`**

Defines the CMake build configuration.

**`.gitignore`**

Excludes generated files and local or sensitive data from version control.

**`README.md`**

Provides the introductory project description and setup instructions.

**`documentation.md`**

Records the evolving architecture, implementation status, technical decisions, and development history.

**`numerick.exe`**

The locally compiled Windows executable. It is generated from the C++ source and should not be treated as source code.

---

## 5. Initial Implementation — Version 0.1.0 (Historical)

The initial version implements a basic interactive command-line application.

### 5.1 Application entry point

The `main()` function constructs a `Numerick` instance and starts the assistant's main loop.

It also catches standard exceptions and reports an error if an exception reaches the application entry point.

### 5.2 Command-line interaction

The application accepts text input through the console.

Each entered command is normalized to lowercase and trimmed of leading and trailing whitespace before being processed.

### 5.3 Command recognition

The current command handler supports these commands:

| Command | Behaviour |
|---|---|
| `hello` | Displays a greeting |
| `hi` | Displays a greeting |
| `hey` | Displays a greeting |
| `help` | Displays the available commands |
| `commands` | Displays the available commands |
| `status` | Displays the current implementation status |
| `time` | Displays the local system time |
| `clear` | Clears the console |
| `exit` | Shuts down the application |
| `quit` | Shuts down the application |

Commands are matched against a predefined set of strings. The current implementation does not understand unrestricted natural-language requests.

### 5.4 Activity logging

On startup, NUMERICK creates the `data` directory if it does not already exist.

It appends activity records to:

```text
data/activity.log
```

The log includes application startup and shutdown events, received input, and unsupported commands, with local timestamps.

The log is intended for basic debugging and activity tracking. It is not yet a persistent conversational memory system.

### 5.5 Status reporting

The `status` command reports the state of the core, AI model, voice input, vision, memory, and system access.

The displayed status explicitly identifies features that have not yet been implemented.

### 5.6 Error handling

The application catches standard exceptions in `main()` and prints the associated error message.

More comprehensive error handling, structured diagnostic logs, and recovery mechanisms remain future work.

---

## 6. Build and Run Instructions

### 6.1 Compiler verification

Run the following command in the project root:

```powershell
g++ --version
```

The current verified compiler version is G++ 16.2.0.

### 6.2 Compile directly with G++

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic src/main.cpp -o numerick.exe
```

This compiles the source file into the Windows executable.

If compilation fails, G++ reports errors and may not produce a new executable.

### 6.3 Run NUMERICK

```powershell
.\numerick.exe
```

### 6.4 Build through CMake

The prepared CMake configuration can also be used:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

Run the CMake-built executable with:

```powershell
.\build\numerick.exe
```

CMake must be installed and available in the terminal for these commands to work.

### 6.5 Inspect the activity log

```powershell
Get-Content .\data\activity.log
```

### 6.6 Initial verification procedure

The following sequence can be used to check the current implementation:

1. Start the application.
2. Enter `hello` and verify that a greeting appears.
3. Enter `status` and verify that the reported capabilities are accurate.
4. Enter `time` and verify that a timestamp appears.
5. Enter an unsupported request and verify that NUMERICK acknowledges its current limitation.
6. Enter `exit` and verify that the application shuts down.
7. Inspect the activity log and verify that the interactions were recorded.

---

## 7. Initial Architecture and Limitations (Historical — Version 0.1.0)

Version 0.1.0 is a deterministic command-line program, not yet an AI-powered assistant.

The current implementation has no:

- Connected AI model or reasoning engine.
- Natural-language intent classification beyond exact command matching.
- Microphone input or wake-word detection.
- Speech recognition or speech synthesis.
- Camera integration or image analysis.
- Web-search integration.
- Long-term conversational memory.
- Windows automation tools.
- Git operation tools.
- Dedicated frontend.
- Automated test suite.

These limitations are intentional at this stage. The initial goal is to establish a working C++ executable and a manageable project structure before introducing external dependencies.

---

## 8. Security and Reliability Principles

Security and reliability must be considered part of the architecture rather than added as an afterthought.

### Permission boundaries

Tools must have explicit, limited capabilities. The AI reasoning layer must not automatically receive unrestricted access to the operating system.

### Verification

Important actions must produce verifiable outcomes. For example, a Git push should be considered successful only after checking the command result and, where appropriate, the remote repository state.

### Human approval

Operations involving significant deletion, external publication, credential exposure, or other consequential effects should require approval until a carefully scoped policy explicitly authorizes them.

### Private information

API keys, tokens, passwords, private logs, and other sensitive data must not be committed to GitHub.

### Uncertainty

NUMERICK must acknowledge missing information, failed tool calls, unclear inputs, and unverified conclusions rather than pretending a task succeeded.

### Medical information

Medicine identification must be grounded in readable label information and reliable references. NUMERICK must distinguish general medicine information from individualized medical advice and must not infer that a medicine is appropriate for the user simply because it has been identified.

---

## 9. Planned Development Milestones

These milestones describe the intended sequence, not a fixed deadline.

### Milestone 1 — C++ foundation

**Status: Implemented; initial verification completed locally.**

- Establish the project structure.
- Compile and run the C++ application.
- Add basic command handling.
- Implement initial activity logging.
- Maintain build instructions and documentation.

### Milestone 2 — AI reasoning foundation

Introduce an AI provider interface and connect a suitable model.

The assistant should be able to interpret natural-language requests and distinguish between answering a question, using a tool, and asking for clarification.

### Milestone 3 — Controlled tool system

Create explicit tool interfaces with defined inputs, outputs, permissions, and error handling.

Begin with low-risk capabilities before adding more powerful operations.

### Milestone 4 — Persistent memory

Add structured local storage, relevant conversation history, and user-approved persistent preferences.

### Milestone 5 — Voice interaction

Introduce wake-word detection, speech recognition or a real-time speech model, and spoken responses.

### Milestone 6 — Visual understanding

Integrate camera capture, image analysis, and OCR. Develop and test the medicine-label workflow with explicit uncertainty handling.

### Milestone 7 — Computer automation

Introduce controlled Windows operations, file handling, application control, and development workflow assistance.

### Milestone 8 — Reliability and interface

Expand automated testing, task recovery, security controls, diagnostics, and eventually the graphical frontend.

Milestones may overlap where appropriate. Each new capability should be implemented and verified in manageable increments.

---

## 10. Git and Version Control

Git will be used to record meaningful development milestones.

The project should follow these practices:

- Commit working milestones with clear messages.
- Test changes before committing.
- Keep generated executables and build directories out of the repository.
- Keep credentials and private runtime data out of commits.
- Update this document whenever architecture, dependencies, capabilities, or development procedures materially change.

The initial remote repository has not yet been created.

### Initial milestone commit

Suggested commit message:

```text
feat: establish Numerick C++ core
```

---

## 11. Development Log

### Version 0.1.0 — Initial C++ core

**Completed:**

- Created the project directory and initial module folders.
- Installed and configured a modern G++ compiler through MSYS2.
- Established a C++17 command-line application.
- Implemented basic command recognition.
- Added system status and local time commands.
- Added local activity logging.
- Added initial CMake configuration.
- Added README and Git-ignore configuration.

**Verification:**

The application compiled and launched successfully on the development machine. The NUMERICK welcome screen appeared, and the application was ready to accept commands.

Individual command tests and automated tests should be recorded separately as they are performed.

**Known issues and outstanding tasks:**

- The AI reasoning engine is not connected.
- The GitHub remote repository is not yet configured.
- Automated tests have not yet been added.


**Verification results — Manual test run**

- [x] Application compiled successfully with G++ 16.2.0.
- [x] Application launched and displayed the welcome screen.
- [x] Greeting commands responded successfully.
- [x] `help` displayed the available commands.
- [x] `status` reported the current implementation state.
- [x] `time` displayed the local system time.
- [x] An unsupported request was handled honestly.
- [x] `exit` shut down the application cleanly.
- [x] Activity logging recorded inputs and shutdown events.

**Result:** Initial manual verification passed.

**Testing limitations:** No automated test suite has been implemented yet. These results reflect manual testing on the development machine.

**Outstanding tasks:**
- Connect an AI reasoning model.
- Create and configure the GitHub remote repository.
- Add automated tests.


### Version 0.3.0 — Command parser and automated tests

**Status: Implemented; initial manual and automated tests passed.**

**Changes:**

- Added a dedicated command parser interface.
- Implemented command recognition in `src/core/command_parser.cpp`.
- Separated command parsing from the main application loop.
- Added automated tests in `tests/command_parser_tests.cpp`.
- Covered command aliases, capitalization, whitespace, AI prompt preservation, empty input, and unknown commands.
- Integrated the parser with the existing local AI client.
- Improved the default AI response instructions and increased the output token limit.
- Added reporting when Ollama indicates that an answer ended because the output limit was reached.
- Updated CMake configuration to include the parser and register its automated tests.

**Verified behaviour:**

- Automated parser tests printed `All command parser tests passed.`
- The application recognized mixed-case commands such as `HeY`.
- AI requests were sent to the local Qwen model and returned complete answers in the latest manual test.
- Empty AI requests produced usage instructions.
- Unsupported commands were handled without attempting arbitrary actions.
- The status and help commands continued to work.
- The application remained interactive after responding.

**Current limitations:**

- Natural-language requests still require the `ask` prefix.
- Conversation history is not maintained across AI requests.
- Voice, visual understanding, persistent memory, and computer automation remain unimplemented.
- AI client failure scenarios need further automated testing.

**Next objective:**

Improve automated test coverage and establish a controlled tool interface before introducing computer automation.



### Version 0.2.0 — Initial local AI integration

**Status: Implemented; initial manual test passed.**

**Changes:**

- Added `AIClient` as a separate C++ module.
- Added an HTTP connection to the local Ollama API using Windows WinHTTP.
- Added JSON request and response handling using nlohmann/json.
- Integrated the Qwen 3.5 4B model through the `ask <question>` command.
- Disabled model thinking in API requests where supported.
- Added a response token limit and generation options.
- Added error handling for HTTP requests and unexpected response formats.
- Avoided recording the complete AI prompt in the activity log.

**Files added:**

- `include/numerick/ai_client.hpp`
- `src/core/ai_client.cpp`

**Files modified:**

- `src/main.cpp`
- `CMakeLists.txt`

**Verified behaviour:**

- The updated C++ application compiled and launched.
- NUMERICK submitted a question to the locally running Ollama service.
- Qwen generated an answer and NUMERICK displayed it.
- The `status`, `help`, and `exit` commands continued to work.

**Model evaluation:**

The Qwen 3.5 4B model completed an earlier direct Ollama test in 1.87 seconds, generating 85 tokens. This was a single measured request, not a guarantee of consistent response latency.

A larger Gemma 4 12B quantized model took approximately 47.47 seconds for a similar direct API request in the tested configuration, including model loading and answer generation. Its generation time was approximately 34.78 seconds.

Qwen 3.5 4B was selected as the initial local model candidate because it fit within the GPU allocation reported by Ollama and responded much faster in the measured test.

**Current limitations:**

- AI requests must be explicitly prefixed with `ask`.
- Conversation history is not yet passed between AI requests.
- Natural voice interaction is not implemented.
- Camera and image analysis are not integrated.
- The AI cannot yet invoke application tools or control the computer.
- Automated tests for the AI client have not yet been implemented.

**Next objective:**

Establish automated tests and a clearer separation between conversation handling, AI requests, and future tool execution before expanding the assistant's capabilities.



---

## 12. Rules for Maintaining This Document

This document is a living engineering record.

Whenever a new feature is introduced:

1. Record its purpose and intended behaviour.
2. Document the relevant source files and architectural changes.
3. Add build, configuration, or usage instructions where necessary.
4. Record the tests performed and their outcomes.
5. Update the limitations and milestone status.
6. Add an entry to the development log when a meaningful milestone is completed.

Do not mark a feature as implemented merely because code has been written. Its actual behaviour must be tested before the documentation describes it as working.

Keep this document factual. Distinguish implemented features from planned work and distinguish successful tests from assumptions.


### Tool Registry — Initial implementation

**Status: Implemented and tested in isolation.**

**Files added:**

- `include/numerick/tool_registry.hpp`
- `src/tools/tool_registry.cpp`
- `tests/tool_registry_tests.cpp`

**Build integration:**

- Registered the tool-registry test executable in CMake.
- Added the tool-registry test to CTest.
- Kept the registry separate from the main assistant executable.
- Preserved the existing AI client and command parser integration.

**Verified behaviour:**

- Registered tools can be discovered by name.
- Tool definitions can be inspected.
- Registered handlers can execute successfully.
- Unknown tools are rejected.
- Non-object JSON arguments are rejected.
- Handler-level validation errors are returned as failures.
- Duplicate tool registrations are rejected.
- Exceptions from handlers are converted into failure results.

**Test results:**

- Command parser tests: Passed.
- Tool registry tests: Passed.
- CTest: 2 of 2 tests passed.
- Main application: Manual verification passed after the CMake build.

**Current limitations:**

- The tool registry is not yet connected to the AI client's response handling.
- AI-generated tool requests are not executed.
- Parameter schemas are stored but not fully validated against JSON Schema.
- No real operating-system tools have been registered.
- Permission policies and approval workflows remain future work.

**Next objective:**

Connect AI-generated tool requests to the registry through a controlled execution layer, beginning with harmless, read-only tools.
---

**End of current documentation — NUMERICK v0.1.0**


### Tool Registry — Initial implementation

**Status: Implemented and tested in isolation.**

#### Files added

- `include/numerick/tool_registry.hpp`
- `src/tools/tool_registry.cpp`
- `tests/tool_registry_tests.cpp`

#### Architecture

Implemented a C++ tool registry that stores tool definitions and their associated handlers.

Each tool definition contains a name, description, and JSON parameter schema. The registry supports registering tools, looking them up, listing their definitions, and executing registered handlers.

The registry is currently separate from the main assistant execution flow. AI-generated tool requests cannot yet invoke it.

#### Tests implemented

The automated test suite covers:

- Registration and lookup of tools.
- Retrieval of registered tool definitions.
- Successful execution of a registered handler.
- Rejection of unknown tools.
- Rejection of non-object JSON arguments.
- Handler-level input validation failures.
- Rejection of duplicate tool registrations.
- Conversion of handler exceptions into failure results.

#### Verification results

- Command parser tests: Passed.
- Tool registry tests: Passed.
- CTest: 2 of 2 tests passed.
- CMake build: Completed successfully.
- Main application: Manual verification passed after the CMake build.
- Local Qwen AI integration: Responded successfully during manual testing.

#### Current limitations

- The registry is not connected to AI-generated tool calls.
- No real operating-system tools are registered.
- Parameter schemas are stored but are not fully validated against JSON Schema.
- A dedicated permission and approval system has not yet been implemented.

#### Next objective

Connect the AI layer to the registry through a controlled execution process. Begin with a harmless, read-only tool, validate its inputs, execute it, and return the result to the model.


### Version 0.4.0 — Initial AI-driven tool execution

**Status: Implemented; initial end-to-end manual test passed.**

#### Changes

- Added a general-purpose `AIClient::chat()` method.
- Added the `ToolAgent` orchestration module.
- Converted registered tool definitions into Ollama's function-tool format.
- Processed structured tool-call requests from the AI model.
- Executed requested tools through the existing registry.
- Returned tool results to the model for a final response.
- Added limits of three tool rounds and four tool calls per request.
- Registered the built-in `get_local_time` tool at application startup.
- Added the `agent <request>` console command.
- Preserved the existing `ask <question>` command for ordinary AI responses.
- Updated the application version to 0.4.0.

#### Verification

- CMake build completed successfully.
- Command parser tests passed.
- Tool registry tests passed.
- Built-in tool tests passed.
- The application displayed one registered tool at startup.
- The `status` command continued to work.
- An `agent` request asking for the current time returned a natural-language answer.
- An ordinary `ask` request continued to return an AI-generated response.
- The application exited normally.

#### Current limitations

- The end-to-end tool workflow has only received initial manual testing.
- Tool-call traces are not yet exposed in a dedicated diagnostic view.
- Full JSON Schema validation is not implemented.
- There is no general-purpose user approval system.
- Only a low-risk local-time tool is registered.
- Voice, camera input, persistent conversational memory, and Windows automation remain unimplemented.

#### Next objective

Add automated tests for the agent orchestration, including successful tool execution, unknown tool requests, tool failures, malformed calls, and execution limits. Improve diagnostics before registering more powerful tools.


### Milestone 4.1 — Automated tool-agent tests

**Status: Implemented and tested.**

#### Changes

- Added the `ChatClient` interface to separate the agent from a specific AI provider.
- Updated `AIClient` to implement the interface.
- Updated `ToolAgent` to depend on the abstract chat interface.
- Added a fake chat client for deterministic agent testing.
- Added `tests/tool_agent_tests.cpp`.
- Registered the new test target with CMake and CTest.

#### Test coverage

- Successful tool execution and delivery of the result back to the model.
- Rejection of unknown tool requests.
- Rejection of malformed tool arguments.
- Enforcement of the tool-round limit.
- Enforcement of the tool-call limit.
- Rejection of empty prompts.

#### Verification results

- CMake build completed successfully.
- Command parser tests: Passed.
- Tool registry tests: Passed.
- Built-in tool tests: Passed.
- Tool agent tests: Passed.
- CTest: 4 of 4 tests passed.
- Direct execution of `tool_agent_tests.exe` reported that all tool-agent tests passed.

#### Current limitations

- The automated agent tests use a fake chat client rather than a live Ollama service.
- Full JSON Schema validation is not implemented.
- Tool permissions and approval policies need further development before adding consequential operations.
- The live tool-calling workflow still requires separate integration testing.

#### Next objective

Improve request validation and diagnostic reporting, then verify the complete agent workflow against the local Ollama model.


### Tool Registry — Schema Validation

**Status: Implemented and tested.**

#### Changes

- Added validation of tool parameter schemas during registration.
- Added checks for required properties and supported property types.
- Added validation of argument types before handler execution.
- Added rejection of missing required arguments.
- Added rejection of unexpected arguments when `additionalProperties` is false.
- Added checks for malformed schema definitions.
- Preserved the existing registry and agent interfaces.

#### Supported validation features

- Top-level object schemas.
- Property definitions and basic JSON types: string, integer, number, boolean, object, array, and null.
- Required-property declarations.
- The `additionalProperties` boolean.
- Required arguments and property types at execution time.

#### Verification

- CMake build: Passed.
- Command parser tests: Passed.
- Tool registry tests: Passed.
- Built-in tool tests: Passed.
- Tool agent tests: Passed.
- CTest: 4 of 4 test suites passed.

The registry tests now also cover missing required arguments, type mismatches, unexpected arguments, and invalid schema definitions.

#### Limitations

This is a deliberately limited schema validator, not a complete JSON Schema implementation. Nested schema validation, numeric ranges, string patterns, enums, array item constraints, and other advanced JSON Schema features are not implemented.

The validator reduces malformed tool requests but does not replace permission checks or approval requirements for consequential operations.


### Milestone 4.2 — Tool validation, diagnostics and safety tests

**Status: Implemented; automated tests and initial live integration test passed.**

#### Tool argument validation

- Added registration-time validation for supported tool schema definitions.
- Added checks for required properties and basic JSON types.
- Rejects missing required arguments.
- Rejects argument type mismatches.
- Rejects undeclared arguments when `additionalProperties` is false.
- Rejects malformed schemas and unsupported property types.

The validator implements a limited subset of JSON Schema rather than the full standard.

#### Agent diagnostics

Added `AgentRunReport` and `ToolCallDiagnostic` to expose:

- Number of model requests.
- Tool rounds and requested tool-call counts.
- Successful, failed and rejected tool calls.
- Tool names and outcome classifications.
- Reasons for stopping when tool-call or tool-round limits are reached.

The application writes diagnostic counts and tool outcomes to the activity log without recording full prompts, argument values, or tool output contents.

#### Automated safety coverage

The tool-agent tests cover:

- Successful tool execution and result delivery.
- Unknown tool names.
- Malformed tool arguments and model responses.
- Registry schema-validation failures.
- Handler failures.
- Empty prompts.
- Tool-round and tool-call limits.
- Confirmation that handlers do not run when execution limits are exceeded.

#### Verification results

- CMake build: Passed.
- Command parser tests: Passed.
- Tool registry tests: Passed.
- Built-in tool tests: Passed.
- Tool-agent tests: Passed.
- CTest: 4 of 4 test suites passed.

#### Live integration test

The local Qwen 3.5 4B model successfully requested the registered `get_local_time` tool.

Recorded diagnostic results:

- Model requests: 2.
- Tool rounds: 1.
- Tool calls requested: 1.
- Tool calls succeeded: 1.
- Tool calls failed: 0.
- Tool calls rejected: 0.
- Stop reason: `completed`.

The tool returned its result, and Qwen generated the final natural-language answer.

#### Remaining limitations

- JSON Schema validation is incomplete.
- The live integration test covers the local-time workflow, not every possible model response.
- No general approval system for consequential actions is implemented.
- No file-modifying or general-purpose operating-system tools have been registered.
- Voice input, camera integration and persistent conversational memory remain unimplemented.

#### Next objective

Continue hardening the live agent workflow and test additional failure cases before introducing tools that access files, repositories or applications.

## 2026-10-25 — Direct-Answer Regression Test and Agent Verification

**Version:** 0.4.0

### Changes
- Added an automated regression test confirming that a direct model response does not execute registered tools.
- Verified that the direct-answer path reports one model request, zero tool rounds, and zero tool calls.
- Verified that tool-call diagnostics correctly track successful execution and completed agent workflows.

### Verification
- All four CTest suites passed.
- Live Ollama verification: a general knowledge question completed in one model request without invoking a tool.
- Live Ollama verification: a current-time question invoked `get_local_time`, returned the result to the model, and completed in two model requests.
- Both live checks reported zero tool failures and zero rejected tool calls.

### Notes
- Automated tests use a fake ChatClient to verify agent behavior deterministically.
- The Ollama checks are manual end-to-end verification, not automated integration tests.

## 2026-10-25 — In-Memory Conversation Sessions

**Version:** 0.4.0

### Changes
- Added bounded, in-memory conversation sessions.
- Retained recent conversation turns across separate `agent` commands.
- Integrated existing conversation history with tool-agent execution.
- Preserved tool interactions in the conversation transcript.
- Added session and agent-conversation test suites.
- Displayed the conversation-history limit at startup.

### Automated verification
- Command parser tests: Passed.
- Tool registry tests: Passed.
- Built-in tool tests: Passed.
- Tool agent tests: Passed.
- Conversation session tests: Passed.
- Tool agent conversation tests: Passed.
- CTest: 6 of 6 test suites passed.

### Live verification
- NUMERICK recalled the fictional project ID `MINT-482` in a subsequent command.
- The `agent` command obtained a current timestamp through `get_local_time`.
- A subsequent request correctly identified `get_local_time` as the tool used.
- Manual verification confirmed context continuity for these scenarios.

### Limitations
- Conversation history is held in RAM and is lost when the application exits.
- The history limit is bounded by turns; it is not a token-based context budget.
- Live testing covered the scenarios listed above, not every possible conversation or tool sequence.

## 2026-10-25 — Persistent Explicit Memory

**Version:** 0.5.0

### Design
- Added a separate `PersistentMemory` module; it does not replace the ten-turn in-memory `ConversationSession`.
- Persistent facts are only added through the explicit `remember <fact>` command.
- Added `memories` to list stored facts and IDs, and `forget <ID>` to delete an individual fact.
- Stored records in `data/memory.json`, which is excluded from version control by the existing `data/` ignore rule.
- Limited the store to 50 entries and each fact to 500 bytes of text.
- Added only the newest 20 saved facts, capped at 4,000 bytes, to `ask` and `agent` system prompts.
- Serialized memory context as JSON and instructed the model to treat saved values as data rather than instructions.
- Avoided logging full fact text to `data/activity.log`.
- Updated the application version to 0.5.0 and added a persistent-memory test target.

### Verification
Record the actual results after testing the local Windows build:
- Clean CMake build: [record result]
- CTest: [record actual number of suites passed]
- Live save/restart/recall/delete/restart check: [record result]

### Limitations
- Persistent memory is not encrypted; the JSON file is local plaintext.
- Facts are saved explicitly, not inferred or saved automatically by the model.
- The model only receives bounded memory context; this is a character limit, not a token-aware budget.
- Memory context can influence responses but does not guarantee correct recall for every prompt.

## 2026-10-25 — Persistent Explicit Memory

**Version:** 0.5.0

### Changes
- Added persistent local memory, separate from temporary conversation history.
- Added `remember <fact>` to save facts explicitly.
- Added `memories` to list saved facts and their IDs.
- Added `forget <ID>` to delete individual facts.
- Persisted memories in `data/memory.json`.
- Added bounded memory context to AI prompts.
- Kept persistent memory separate from the activity log.

### Verification
- Live test: A saved fact survived application exit and restart.
- Live test: NUMERICK recalled the saved fictional project ID `MINT-482`.
- Deletion across restarts: Pending verification.
- Automated tests: Record the actual CTest results here.

### Limitations
- Saved memory is local plaintext and is not encrypted.
- Facts are saved explicitly rather than automatically.
- Memory context is bounded; recall is not guaranteed for every question.

## 2026-10-25 — Scoped Read-Only Project File Tools

**Version:** 0.6.0

### Changes
- Added `list_project_files` for enumerating safe entries under a configured workspace root.
- Added `read_project_file` for reading allowlisted text files.
- Defaulted the workspace root to the process working directory, with `NUMERICK_WORKSPACE_ROOT` as an explicit override.
- Rejected absolute paths and parent-directory traversal, resolved candidate paths and checked that they remain inside the workspace.
- Excluded private/runtime and build folders, common credential/key/database/binary files, and non-allowlisted extensions.
- Capped file reads at 256 KiB, returned tool content at 12 KiB, and limited directory listings to 80 entries.
- Added tests for safe listing/reading, traversal, secret filtering, binary/oversized files, invalid path kinds, and symlink escape where symlink creation is supported.
- Kept file access read-only; no write, delete, process-execution, or arbitrary shell tools were added.

### Verification
- CMake build: Record actual result.
- CTest: Record actual results, including the `project_file_tools_tests` suite.
- Live Ollama file inspection: Record only after testing it on the target machine.

### Limitations
- The allowlist and blocked-path rules are defense-in-depth, not a complete OS sandbox.
- File access remains limited to the selected workspace root and allowlisted text formats.
- File content is untrusted data; the agent is instructed not to follow embedded instructions.