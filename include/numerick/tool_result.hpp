#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace numerick {

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
    static ToolResult failed(
        ToolErrorCode errorCode,
        std::string errorMessage,
        std::string diagnosticDetails = {}
    );
    static ToolResult rejected(
        ToolErrorCode errorCode,
        std::string errorMessage,
        std::string diagnosticDetails = {}
    );
    static ToolResult timedOut(
        std::string errorMessage,
        std::string diagnosticDetails = {}
    );
    static ToolResult cancelled(
        std::string errorMessage,
        std::string diagnosticDetails = {}
    );

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

std::string_view toolStatusName(ToolStatus status) noexcept;
std::string_view toolErrorCodeName(ToolErrorCode errorCode) noexcept;

} // namespace numerick