# NUMERICK — Project Documentation

**Project name:** NUMERICK  
**Current version:** 0.1.0  
**Project stage:** Initial C++ core  
**Primary language:** C++  
**Platform:** Windows  
**Build system:** Direct G++ compilation; CMake configuration prepared  
**Documentation status:** Living document — update as the project evolves

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

## 5. Current Implementation — Version 0.1.0

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

## 7. Current Architecture and Limitations

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

---

**End of current documentation — NUMERICK v0.1.0**
