# NUMERICK — Phase 1A Tool Execution Audit

**Audit date:** 8 November 2026

**Source baseline:** `main` at `a81fc47`

**Audit branch:** `docs/phase-1a-tool-execution-audit`

**Audit type:** Source inspection only

**Implementation status:** No application source code changed

## 1. Purpose and scope

This document records the existing tool execution architecture before changes are made to its interfaces or behavior.

The audit covers:

- Tool definitions, registration and argument validation.
- Tool handler execution and result/error behavior.
- ToolAgent orchestration, limits and diagnostics.
- Application tool registration and current tool catalog.
- Existing automated tests and their coverage.
- Current timeout, cancellation, permission and logging boundaries.

This is a baseline, not a proposed implementation. The design of a replacement result contract remains a separate task.

## 2. Architecture overview

### 2.1 Registration and initialization

`src/main.cpp`, in the `Numerick` constructor, creates the registry and agent, then registers the available tool groups:

1. `registerBuiltinTools(registry_)`
2. `registerApplicationTools(registry_)`
3. `registerProjectFileTools(registry_, workspaceRoot_)`

The agent holds a reference to the registry. The registry owns the registered definitions and handler functions.

### 2.2 Normal tool execution path

The normal model-directed flow is:

1. `ToolAgent` obtains registered tool definitions from `ToolRegistry`.
2. Definitions are converted into the function-tool format expected by `ChatClient`.
3. `ChatClient::chat(messages, tools)` makes a synchronous model request.
4. `ToolAgent` validates the shape of the returned message and tool-call list.
5. It checks that each call has a string function name and object arguments.
6. It rejects malformed requests and unknown tool names.
7. Registered calls are submitted to `ToolRegistry::execute`.
8. The registry validates arguments and invokes the handler.
9. A tool result is appended to the conversation as a tool message.
10. The agent requests another model response or returns a final answer.

Tool calls in a batch are processed sequentially.

### 2.3 Special deterministic path

`ToolAgent` has a dedicated path for explicit C++ member-relationship questions when `inspect_cpp_member_relationship` is registered.

It extracts class/member pairs from the user prompt, invokes the registry directly and returns the evidence-bearing result without asking the model to paraphrase it.

This path bypasses the ordinary model/tool-call loop. It still records a requested tool call and its success/failure outcome.

## 3. Current public interfaces

### 3.1 Tool definitions

Source: `include/numerick/tool_registry.hpp`

```cpp
struct ToolDefinition {
    std::string name;
    std::string description;
    Json parametersSchema;
};
```

The definition stores a name, description and JSON parameter schema.

### 3.2 Tool result

Current implementation:

```cpp
struct ToolResult {
    bool success = false;
    std::string output;
};

using ToolHandler =
    std::function<ToolResult(const Json& arguments)>;
```

The result has only a success flag and a text output. It has no separate structured data, stable error code, retryability field, execution ID, duration or timing metadata.

A handler can return `{false, "message"}` for a failure. The registry returns that result to the caller without converting it into a structured error type.

### 3.3 Tool registry interface

`ToolRegistry` currently exposes:

- `registerTool(definition, handler)`
- `contains(name)`
- `definitions()`
- `execute(name, arguments)`

Registered tools are stored in a `std::map` keyed by tool name.

## 4. Registration validation

Source: `src/tools/tool_registry.cpp`

`registerTool` rejects:

- Empty or invalid names. Names may contain letters, digits and underscores.
- Empty descriptions.
- Invalid or unsupported parameter schema definitions.
- Empty handler functions.
- Duplicate tool names.

Registration failures throw `std::invalid_argument`.

### 4.1 Supported schema subset

The current registry implements a limited subset of JSON Schema.

At the top level:

- The schema must be an object with `"type": "object"`.
- `properties`, if present, must be an object.
- `required`, if present, must be an array of strings.
- Every required name must exist in `properties`.
- `additionalProperties`, if present, must be a boolean.

For individual properties:

- Each property schema must be an object.
- A declared `type` must be a string.
- Supported types are `string`, `integer`, `number`, `boolean`, `object`, `array` and `null`.

This is not a complete JSON Schema validator. Keywords outside the implemented subset are not enforced by this code.

## 5. Runtime argument validation and execution

Before calling a handler, `ToolRegistry::execute`:

1. Checks that the tool exists.
2. Requires the arguments value to be a JSON object.
3. Checks that all required fields are present.
4. Rejects undeclared fields when `additionalProperties` is `false`.
5. Checks the JSON type of declared properties that have a `type`.

If `additionalProperties` is absent, the runtime behavior defaults to allowing additional fields.

If a property schema does not declare a type, this validator does not apply a type check to that property.

### 5.1 Unknown tools and failures

- Unknown tool names return `{false, "Tool is not registered: ..."}`.
- Invalid arguments return `{false, error_message}`.
- Standard handler exceptions are caught and returned as `"Tool execution failed: " + error.what()`.
- Unknown exceptions are caught and returned as a generic execution-failure message.
- A handler-returned failure is passed through as its existing `ToolResult`.

The returned error information is plain text. There is no structured distinction between validation errors, permission failures, unavailable resources, timeouts and internal failures.

## 6. Current registered tool catalog

The application registers nine tools.

| Tool | Registration group | Current purpose |
|---|---|---|
| `get_local_time` | Built-in | Returns local date/time; accepts no arguments |
| `open_application` | Application | Launches a constrained Windows application target |
| `list_project_files` | Project files | Lists accessible workspace entries |
| `read_project_file` | Project files | Reads an allowlisted text file |
| `search_project_files` | Project files | Searches accessible text files |
| `find_cpp_symbol` | Project files | Finds bounded whole-identifier occurrences |
| `inspect_cpp_member_relationship` | Project files | Produces heuristic source evidence for member relationships |
| `inspect_cpp_component` | Project files | Produces a bounded cross-file component report |
| `describe_project_architecture` | Project files | Produces a bounded workspace overview |

Project-file tools are read-only. Their implementation canonicalizes the configured workspace root and applies path, traversal, blocked-folder, file-type and size restrictions.

`open_application` is constrained to application resolution and launch behavior. Its success message indicates that a launch request was submitted; it is not proof that the expected application window was opened and verified.

## 7. ToolAgent execution, limits and diagnostics

Sources:

- `include/numerick/tool_agent.hpp`
- `src/core/tool_agent.cpp`

### 7.1 Default limits

The constructor defaults are:

| Limit | Default |
|---|---:|
| Maximum tool rounds | 3 |
| Maximum tool calls | 4 |

The round limit applies to tool-call batches. The call limit applies to the total number of requested calls in the run.

The agent checks whether the latest batch would exceed the remaining call budget before executing it. It does not partially execute a batch that exceeds the limit.

`toolCallsRequested` is incremented before the limit checks, so it can include calls that were requested but not executed.

### 7.2 AgentRunReport

The current report contains:

- Final `answer`.
- `modelRequests`.
- `toolRounds`.
- `toolCallsRequested`.
- `toolCallsSucceeded`.
- `toolCallsFailed`.
- `toolCallsRejected`.
- `stopReason`.
- A vector of tool name/outcome pairs.
- `turnMessages` for the messages added during the current turn.

`AgentStopReason` currently has `Completed`, `ToolRoundLimitReached` and `ToolCallLimitReached`.

`ToolCallDiagnostic` contains only a tool name and an outcome string. It does not contain structured error details, arguments, timing, duration or an execution identifier.

### 7.3 Agent-side request validation

The agent validates the returned model response and tool-call structure before execution.

- Invalid response/message shapes throw runtime errors.
- A malformed tool-call list is rejected.
- Each call requires a function object, string name and object arguments.
- Malformed calls and unknown tool names are counted as rejected.
- Registry validation and handler failures are counted as failed calls.
- Successful calls are counted separately.

The normal model-directed flow sends tool output back as text. Failed tool output is prefixed with `"Tool error: "`.

## 8. Timeout and cancellation boundaries

A distinction is required between model-request timeouts and tool-execution timeouts.

### 8.1 Model transport timeout

`src/core/ai_client.cpp` configures WinHTTP timeouts for the model HTTP request. The code sets resolve, connect and send timeouts and uses the configured `responseTimeoutMs` for the response timeout.

This protects the HTTP/model request boundary. It does not impose a timeout on an individual tool handler.

### 8.2 Tool execution

The inspected tool interfaces do not expose:

- A per-tool deadline or timeout.
- A cancellation token or stop context.
- A global cancellation mechanism for active tool execution.
- A standard way for a handler to report cancellation.
- An execution boundary that can safely stop every currently registered handler.

The `ChatClient::chat` interface is synchronous and has no cancellation parameter. Tool handlers are also invoked synchronously by the registry.

## 9. Existing tests

The CMake project registers three directly relevant test targets.

| CTest target | Source file | Existing coverage |
|---|---|---|
| `tool_registry_tests` | `tests/tool_registry_tests.cpp` | Registration, lookup, successful execution, unknown names, argument validation, duplicate registration, invalid schemas and handler exceptions |
| `tool_agent_tests` | `tests/tool_agent_tests.cpp` | Successful tool cycles, unknown/malformed calls, schema and handler failures, malformed model responses, round/call limits, relationship routing and direct answers |
| `tool_agent_conversation_tests` | `tests/tool_agent_conversation_tests.cpp` | Conversation validation, direct-answer transcripts, tool/result transcripts, limit-stop transcript and empty assistant responses |

These targets are defined in `CMakeLists.txt` and use a fake chat client for agent tests, so those tests do not require a live Ollama service.

### 9.1 Identified test gaps

The inspected tests do not establish coverage for:

- Per-tool timeout behavior.
- Cancellation before or during handler execution.
- Repeated-call or idempotency policies.
- Structured error-code mapping.
- Execution metadata and duration reporting.
- Permission-denied and confirmation-required outcomes.
- Redaction of sensitive arguments and internal paths in diagnostics.
- Recovery when a tool depends on an unavailable or terminated external resource.

This audit records test coverage visible in source. It does not claim that the tests were freshly executed during this documentation-only inspection.

## 10. Current logging and observability

`src/main.cpp` opens `data/activity.log` in append mode. Log lines include a timestamp and a text message; the stream is flushed after writes.

For text-agent requests, the console and activity log include aggregate model-request, tool-round and tool-call counts, stop reason, and tool name/outcome entries.

The continuous-voice path prints a smaller aggregate diagnostic line containing model requests, tool rounds and tool calls. It does not currently emit the same detailed per-tool outcome list as the text-agent path.

The current log is not a structured event stream. The inspected execution report does not record per-tool start/end timestamps or durations.

## 11. Language and component boundary policy

The main application currently uses C++17 and CMake. The repository also retains a Rust voice-service component.

This audit does not require future functionality to be written in C++.

Language choice is component-specific:

- C++ remains appropriate for the existing core and native Windows integration.
- Python may be used for AI/ML workloads, rapid prototyping, automation or orchestration when its ecosystem and development speed offer a real advantage.
- Rust may be used for isolated services or workers when its safety and concurrency properties justify the boundary.
- Other languages may be evaluated when they provide a concrete benefit.

A new language should be introduced deliberately. Evaluate runtime and packaging complexity, dependency management, debugging, maintainability, security, latency, resource use and interoperability. Use a narrow and documented boundary such as a process/service protocol, IPC, FFI or a library interface. Do not rewrite stable components solely to change languages.

## 12. Main gaps carried into Phase 1A

The existing implementation provides a useful working foundation, but the following gaps remain:

1. `ToolResult` has no structured error code, separate data field or execution metadata.
2. Error categories are conveyed as free-form strings.
3. Tool handlers have no standard per-tool timeout or cancellation interface.
4. The agent has no standard execution context that carries deadlines, cancellation or permission decisions.
5. The registry and agent do not expose a common stable execution event model.
6. Diagnostic reports lack per-tool duration, start/end timestamps and structured failure details.
7. Raw exception messages can be forwarded as user/model-visible text; a safer public error mapping is not established.
8. Existing call limits bound the number of calls, but do not bound the duration of an individual handler.

## 13. Design questions before implementation

These questions must be resolved in the ToolResult contract design before changing public interfaces:

- Should a result carry structured `data`, human-readable `message`, or both?
- Which stable error categories are required, and which are retryable?
- How should expected handler failures differ from unexpected internal exceptions?
- What execution metadata is required for logs and diagnostics?
- How should deadlines and cancellation be propagated into handlers?
- What is the behavior when cancellation arrives after an external action has already started?
- How should sensitive arguments, paths and exception details be redacted?
- How will existing tools be migrated without changing their behavior unexpectedly?

Permission levels and user confirmation policy belong to Phase 1B. This audit does not define or implement that policy.

## 14. Scope boundary

This audit does not change runtime behavior. It does not implement a new ToolResult, timeout, cancellation, permission model, logging system, new language runtime or new tool.

The next implementation task should follow only after this audit is reviewed and the proposed result/error contract is agreed.

## 15. Source map

- `include/numerick/tool_registry.hpp`
- `src/tools/tool_registry.cpp`
- `include/numerick/tool_agent.hpp`
- `src/core/tool_agent.cpp`
- `include/numerick/chat_client.hpp`
- `src/core/ai_client.cpp`
- `src/main.cpp`
- `src/tools/builtin_tools.cpp`
- `src/tools/application_tools.cpp`
- `src/tools/project_file_tools.cpp`
- `tests/tool_registry_tests.cpp`
- `tests/tool_agent_tests.cpp`
- `tests/tool_agent_conversation_tests.cpp`
- `CMakeLists.txt`
- `DECISIONS.md`
- `NUMERICK_MASTER_CHECKLIST.md`
