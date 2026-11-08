# NUMERICK – ToolResult Contract Design

**Phase:** 1A – Tool Execution Foundation
**Status:** Approved for implementation
**Date:** 8 November 2026
**Baseline:** `main` at `c91a582`
**Scope:** Contract design only; no production implementation changes.

---

## 1. Purpose

Define a consistent result contract for every NUMERICK tool.

The contract must distinguish successful execution, operational failure, rejected requests, timeout, and cancellation. It must preserve successful tool output while providing structured error information and execution metadata.

## 2. Execution statuses

| Status | Meaning |
|---|---|
| `Succeeded` | The operation completed successfully. |
| `Failed` | The operation was attempted but encountered an operational error. |
| `Rejected` | The request was not accepted because it was invalid or not permitted. |
| `TimedOut` | The operation exceeded an enforced execution deadline. |
| `Cancelled` | The operation was explicitly cancelled. |

A rejected request is not an execution failure. In particular, malformed requests and invalid arguments must not be reported as handler failures when the handler was never invoked.

## 3. Error codes

| Error code | Intended use |
|---|---|
| `None` | Successful result; no error. |
| `MalformedRequest` | Tool-call request does not have the required structure. |
| `UnknownTool` | Requested tool is not registered. |
| `InvalidArguments` | Arguments do not satisfy the tool's requirements. |
| `PermissionDenied` | An operation is denied by a permission policy. |
| `ResourceNotFound` | Requested file, application, or resource does not exist. |
| `ResourceUnavailable` | A required resource cannot currently be accessed. |
| `ResourceLimitExceeded` | An operation exceeds an explicit size or resource limit. |
| `IoError` | File or other input/output operation failed. |
| `LaunchFailed` | An application launch request could not be submitted. |
| `ExecutionFailed` | An operation failed for another operational reason. |
| `Timeout` | An enforced execution deadline was exceeded. |
| `Cancelled` | Execution was cancelled. |
| `InternalError` | An unexpected internal error occurred. |

Error codes are stable machine-readable identifiers. Human-readable wording may evolve without changing the code.

## 4. Proposed data model

Conceptual C++ interface (required standard-library headers):

    #include <chrono>
    #include <optional>
    #include <string>

    enum class ToolStatus {
        Succeeded,
        Failed,
        Rejected,
        TimedOut,
        Cancelled
    };

    enum class ToolErrorCode {
        None,
        MalformedRequest,
        UnknownTool,
        InvalidArguments,
        PermissionDenied,
        ResourceNotFound,
        ResourceUnavailable,
        ResourceLimitExceeded,
        IoError,
        LaunchFailed,
        ExecutionFailed,
        Timeout,
        Cancelled,
        InternalError
    };

    struct ToolExecutionMetadata {
        std::optional<std::chrono::milliseconds> duration;
        bool handlerInvoked = false;
    };

    class ToolResult {
    public:
        static ToolResult succeeded(std::string output);
        static ToolResult failed(ToolErrorCode errorCode, std::string errorMessage, std::string diagnosticDetails = {});
        static ToolResult rejected(ToolErrorCode errorCode, std::string errorMessage, std::string diagnosticDetails = {});
        static ToolResult timedOut(std::string errorMessage, std::string diagnosticDetails = {});
        static ToolResult cancelled(std::string errorMessage, std::string diagnosticDetails = {});

        ToolStatus status() const noexcept;
        const std::string& output() const noexcept;
        ToolErrorCode errorCode() const noexcept;
        const std::string& errorMessage() const noexcept;
        const std::string& diagnosticDetails() const noexcept;
        const ToolExecutionMetadata& execution() const noexcept;

    private:
        friend class ToolRegistry;
        ToolResult() = default;

        void attachExecutionMetadata(ToolExecutionMetadata metadata) noexcept;

        ToolStatus status_ = ToolStatus::Failed;
        std::string output_;
        ToolErrorCode errorCode_ = ToolErrorCode::InternalError;
        std::string errorMessage_;
        std::string diagnosticDetails_;
        ToolExecutionMetadata execution_;
    };

The final implementation should use factory methods to construct valid results rather than relying on arbitrary aggregate initialization.

Proposed factory operations:

- `succeeded(output)`
- `failed(errorCode, errorMessage, diagnosticDetails = {})`
- `rejected(errorCode, errorMessage, diagnosticDetails = {})`
- `timedOut(errorMessage, diagnosticDetails = {})`
- `cancelled(errorMessage, diagnosticDetails = {})`

Factories construct the semantic result and do not accept execution metadata from handlers.

`ToolResult` should be implemented as a class rather than a publicly mutable aggregate:

- Factory methods construct valid result states.
- Read-only accessors expose status, output, error code, error message, diagnostic details, and execution metadata.
- Execution metadata remains private to `ToolResult`.
- `ToolRegistry` is the only component authorized to attach execution metadata.
- Agent-level rejections that never enter the registry retain unavailable duration and `handlerInvoked = false`.

An `isSuccess()` helper may be provided. The status remains the authoritative outcome.
### Factory validation rules

Factories must enforce the status and error-code combinations defined in Section 6. Invalid combinations must be rejected during construction, for example by throwing `std::invalid_argument`.

| Factory | Required status | Permitted error code |
|---|---|---|
| `succeeded` | `Succeeded` | `None` |
| `failed` | `Failed` | `ResourceNotFound`, `ResourceUnavailable`, `IoError`, `LaunchFailed`, `ExecutionFailed`, `InternalError` |
| `rejected` | `Rejected` | `MalformedRequest`, `UnknownTool`, `InvalidArguments`, `PermissionDenied`, `ResourceLimitExceeded` |
| `timedOut` | `TimedOut` | `Timeout` |
| `cancelled` | `Cancelled` | `Cancelled` |

The `succeeded` factory clears all error fields. Non-success factories leave successful output empty. Error messages must be non-empty for non-success results.

## 5. Field semantics

| Field | Contract |
|---|---|
| `status` | Authoritative execution outcome. |
| `output` | Successful tool output, retained as a string for compatibility with current tools. |
| `errorCode` | Stable machine-readable error category. |
| `errorMessage` | Safe, human-readable explanation suitable for the model and user. |
| `diagnosticDetails` | Optional internal troubleshooting details. |
| `execution.duration` | Optional elapsed time for a measured registry execution attempt; absent when no registry attempt occurred. |
| `execution.handlerInvoked` | Whether the registered tool handler was entered. |

### Result invariants

- `Succeeded` must use `ToolErrorCode::None`.
- Non-success statuses must use a meaningful non-`None` error code.
- Successful results must not contain an error message. Non-successful results leave output empty and use errorMessage for the safe explanation.
- Error messages must not depend on parsing diagnostic details.
- Diagnostic details must not be forwarded blindly to the model or displayed to the user.
- `handlerInvoked` records actual control flow independently of status. It is true whenever the registered handler was entered, including when a permission check inside that handler rejects the operation. Registry lookup and schema-validation rejections occur before handler entry and therefore use false.
- A handler exception must report `handlerInvoked = true`.
- Empty successful output is permitted where a tool legitimately has no textual output.

## 6. Outcome mapping

| Existing situation | Proposed status | Proposed error code |
|---|---|---|
| Successful handler execution | `Succeeded` | `None` |
| Malformed model tool request | `Rejected` | `MalformedRequest` |
| Unknown tool | `Rejected` | `UnknownTool` |
| Schema or argument validation failure | `Rejected` | `InvalidArguments` |
| Permission policy denial | `Rejected` | `PermissionDenied` |
| Resource does not exist | `Failed` | `ResourceNotFound` |
| Resource cannot be accessed | `Failed` | `ResourceUnavailable` |
| Configured resource or operation limit exceeded | `Rejected` | `ResourceLimitExceeded` |
| File input/output error | `Failed` | `IoError` |
| Application launch request fails | `Failed` | `LaunchFailed` |
| Other handler failure | `Failed` | `ExecutionFailed` |
| Unexpected handler exception | `Failed` | `InternalError` |
| Enforced deadline exceeded | `TimedOut` | `Timeout` |
| Explicit cancellation | `Cancelled` | `Cancelled` |

Specific tool errors should use the most appropriate code rather than collapsing everything into `ExecutionFailed`.

## 7. Execution metadata ownership

The registry exclusively owns execution metadata for calls submitted to `ToolRegistry::execute`.

- Measure duration with `std::chrono::steady_clock`, which is appropriate for elapsed-time measurement and is not affected by wall-clock adjustments.
- Start timing when `ToolRegistry::execute` begins. Include registry lookup, argument validation, and handler execution when entered.
- Record a measured duration for every registry submission, including unknown-tool and argument-validation rejections. A measured duration of zero is valid if the operation completes within the clock's millisecond resolution.
- Represent duration as unavailable (`std::nullopt`) for agent-level requests rejected before registry submission; do not substitute a measured zero.
- Set `handlerInvoked = true` immediately before entering the registered handler. The registry records this fact even when the handler returns `Rejected` or throws an exception.
- Registry lookup and argument-validation rejections have `handlerInvoked = false`.
- A rejection performed inside a registered handler has `handlerInvoked = true`.
- The registry attaches measured metadata to the final result on every return path, including exception paths. Individual tool handlers must not supply or overwrite it.

Timeout and cancellation enforcement are future implementation work. Adding statuses to the type does not itself provide timeout or cancellation behavior.

## 8. ToolAgent integration

ToolAgent must classify outcomes consistently:

- `Succeeded` increments successful calls.
- `Failed` increments failed calls.
- `Rejected` increments rejected calls.
- `TimedOut` increments a dedicated timeout counter.
- `Cancelled` increments a dedicated cancellation counter.
- Timed-out and cancelled calls must not be silently folded into the ordinary failed-call counter.
- Each per-tool diagnostic should retain the tool name, typed status, error code, duration when available, handler-invocation flag, and safe error message when present.

The existing Ollama conversation protocol must remain compatible. Tool messages continue carrying the tool name and textual content.

Successful calls forward `output`. Non-success calls forward a safe error message, not internal diagnostic details.

The existing deterministic C++ member-relationship route must use the same result contract as ordinary tool calls.

## 9. Diagnostics and privacy

User/model-facing messages and internal diagnostics are separate channels.

- `errorMessage` may be included in the model-facing tool response.
- `diagnosticDetails` is restricted to controlled local diagnostics.
- Diagnostic details must not automatically enter conversation history.
- Logs must avoid exposing secrets, credentials, or unnecessarily sensitive file contents.
- Diagnostic reporting should retain the stable error code and execution outcome.

## 10. Compatibility and migration

The contract migration must cover:

1. `tool_registry.hpp` and `tool_registry.cpp`.
2. `tool_agent.hpp` and `tool_agent.cpp`.
3. All registered tool handlers.
4. `AgentRunReport` and per-tool diagnostics.
5. Existing registry, agent, conversation, built-in, and project-file tests.
6. Console and activity-log diagnostics.

Existing successful textual outputs must remain unchanged unless a separately justified behavior change is approved.

The Ollama request/response protocol and existing tool definitions should not be redesigned as part of this migration.

## 11. Explicitly out of scope

This contract does not itself implement:

- Permission prompts or user confirmation.
- Asynchronous tool execution.
- Cancellation tokens or cancellation propagation.
- Enforced tool deadlines.
- Automatic retries.
- Arbitrary structured JSON tool output.
- A new logging framework.
- A new programming language or runtime.

These must be introduced in their appropriate phases with separate design and tests.

## 12. Acceptance criteria

The contract is ready for implementation when:

- Every status has unambiguous semantics.
- Error codes are stable and mapped consistently.
- Result invariants are enforceable through construction APIs.
- Handler invocation and duration semantics are clear.
- Internal diagnostics are separated from model/user-facing messages.
- ToolAgent counters and diagnostics cover every status.
- Existing textual tool outputs and Ollama conversation behavior remain compatible.
- A migration and test plan covers all current result consumers.


## 13. Agent accounting and diagnostic representation

The following counters must have precise, mutually consistent meanings:

| Field | Meaning |
|---|---|
| `toolCallsRequested` | Total tool calls requested by the model, including calls later blocked by agent limits. |
| `toolCallsProcessed` | Derived metric: sum of the five mutually exclusive outcome counters; not stored independently. |
| `toolCallsSucceeded` | Processed calls with `Succeeded` status. |
| `toolCallsFailed` | Processed calls with `Failed` status only. |
| `toolCallsRejected` | Processed calls with `Rejected` status. |
| `toolCallsTimedOut` | Processed calls with `TimedOut` status. |
| `toolCallsCancelled` | Processed calls with `Cancelled` status. |
| `toolCallsNotExecuted` | Derived metric: requested calls minus processed calls; includes calls left unprocessed by agent limits or intentional early returns. |

The five outcome counters must be mutually exclusive and satisfy:

`toolCallsProcessed = toolCallsSucceeded + toolCallsFailed + toolCallsRejected + toolCallsTimedOut + toolCallsCancelled`

Also:

`toolCallsRequested = toolCallsProcessed + toolCallsNotExecuted`

Malformed and unknown-tool requests that the agent explicitly classifies are processed rejections, even though no registered handler is invoked.

Calls blocked by an agent-level round or call budget are not `ToolResult` failures or rejections. They remain not executed and are explained by the agent stop reason.

### Canonical diagnostic names

Tool status and error code values must have stable textual representations for logs and diagnostics. They must not be serialized as enum integer ordinals.

Proposed status names:

- `succeeded`
- `failed`
- `rejected`
- `timed_out`
- `cancelled`

Proposed error-code names use stable `snake_case`, such as `invalid_arguments`, `permission_denied`, `resource_not_found`, and `internal_error`.

Per-tool diagnostics should store typed status and error-code values internally. Console and log formatting should convert them through centralized name-conversion functions.


### Accounting during early returns

An intentional early return while processing a batch must account for every remaining requested call as not executed. This includes the deterministic C++ member-relationship route when it returns after a successful result while other calls remain in the same batch.

Per-tool diagnostics represent calls assigned a terminal `ToolResult` status. Calls not executed must not receive a fabricated `ToolStatus`; their count is represented by the derived `toolCallsNotExecuted` metric and the applicable stop or early-return context.

`toolCallsProcessed` and `toolCallsNotExecuted` are derived values, not independently mutable counters. This avoids inconsistent totals.


## 14. Implementation sequence

Implementation will proceed in small, independently testable changes.

### Step 1 – Core result type
- Add `ToolStatus` and `ToolErrorCode`.
- Implement the private-state `ToolResult` class and its factories.
- Add centralized status and error-code name conversion.
- Test factory invariants and invalid combinations.

### Step 2 – Tool registry
- Update `ToolRegistry::execute` to return structured outcomes.
- Measure duration using `std::chrono::steady_clock`.
- Attach registry-owned metadata on every return path.
- Distinguish unknown tools, invalid arguments, handler rejection, and exceptions.

### Step 3 – Existing tool migration
- Migrate all nine registered tools to the factory API.
- Assign specific error codes where evidence supports them.
- Preserve successful textual output and existing tool behavior.

### Step 4 – ToolAgent integration
- Convert malformed and unknown model requests into structured rejections.
- Handle all five statuses consistently.
- Add timeout and cancellation counters without claiming enforcement exists.
- Implement derived processed and not-executed accounting.
- Account for early returns and unprocessed calls in a model batch.

### Step 5 – Diagnostics and conversation compatibility
- Expand per-tool diagnostics with typed status, error code, duration, and handler invocation.
- Update console output and activity logging.
- Keep internal diagnostic details out of model-facing messages and conversation history.
- Preserve the existing Ollama tool-message protocol.

### Step 6 – Regression tests
- Update registry, agent, conversation, built-in, and project-file tests.
- Cover all five statuses and factory invariants.
- Verify counter equations and early-return accounting.
- Verify existing successful tool outputs remain unchanged.

### Step 7 – Integration verification
- Build the project.
- Run the complete CTest suite.
- Run focused manual checks with the existing local Ollama setup.
- Record actual results; do not claim timeout or cancellation enforcement until separately implemented and tested.

Each step must compile and pass its relevant tests before the next step begins. No unrelated refactoring is included.
