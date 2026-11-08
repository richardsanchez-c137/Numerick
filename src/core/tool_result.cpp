#include "numerick/tool_result.hpp"

#include <stdexcept>
#include <utility>

namespace numerick {
namespace {

bool isFailedErrorCode(ToolErrorCode code) noexcept {
    switch (code) {
    case ToolErrorCode::ResourceNotFound:
    case ToolErrorCode::ResourceUnavailable:
    case ToolErrorCode::IoError:
    case ToolErrorCode::LaunchFailed:
    case ToolErrorCode::ExecutionFailed:
    case ToolErrorCode::InternalError:
        return true;
    default:
        return false;
    }
}

bool isRejectedErrorCode(ToolErrorCode code) noexcept {
    switch (code) {
    case ToolErrorCode::MalformedRequest:
    case ToolErrorCode::UnknownTool:
    case ToolErrorCode::InvalidArguments:
    case ToolErrorCode::PermissionDenied:
    case ToolErrorCode::ResourceLimitExceeded:
        return true;
    default:
        return false;
    }
}

void requireErrorMessage(const std::string& message) {
    if (message.empty()) {
        throw std::invalid_argument(
            "A non-success ToolResult requires an error message."
        );
    }
}

} // namespace

ToolResult ToolResult::succeeded(std::string output) {
    ToolResult result;
    result.status_ = ToolStatus::Succeeded;
    result.output_ = std::move(output);
    result.errorCode_ = ToolErrorCode::None;
    return result;
}

ToolResult ToolResult::failed(
    ToolErrorCode errorCode,
    std::string errorMessage,
    std::string diagnosticDetails
) {
    if (!isFailedErrorCode(errorCode)) {
        throw std::invalid_argument(
            "Invalid error code for a Failed ToolResult."
        );
    }
    requireErrorMessage(errorMessage);

    ToolResult result;
    result.status_ = ToolStatus::Failed;
    result.errorCode_ = errorCode;
    result.errorMessage_ = std::move(errorMessage);
    result.diagnosticDetails_ = std::move(diagnosticDetails);
    return result;
}

ToolResult ToolResult::rejected(
    ToolErrorCode errorCode,
    std::string errorMessage,
    std::string diagnosticDetails
) {
    if (!isRejectedErrorCode(errorCode)) {
        throw std::invalid_argument(
            "Invalid error code for a Rejected ToolResult."
        );
    }
    requireErrorMessage(errorMessage);

    ToolResult result;
    result.status_ = ToolStatus::Rejected;
    result.errorCode_ = errorCode;
    result.errorMessage_ = std::move(errorMessage);
    result.diagnosticDetails_ = std::move(diagnosticDetails);
    return result;
}

ToolResult ToolResult::timedOut(
    std::string errorMessage,
    std::string diagnosticDetails
) {
    requireErrorMessage(errorMessage);

    ToolResult result;
    result.status_ = ToolStatus::TimedOut;
    result.errorCode_ = ToolErrorCode::Timeout;
    result.errorMessage_ = std::move(errorMessage);
    result.diagnosticDetails_ = std::move(diagnosticDetails);
    return result;
}

ToolResult ToolResult::cancelled(
    std::string errorMessage,
    std::string diagnosticDetails
) {
    requireErrorMessage(errorMessage);

    ToolResult result;
    result.status_ = ToolStatus::Cancelled;
    result.errorCode_ = ToolErrorCode::Cancelled;
    result.errorMessage_ = std::move(errorMessage);
    result.diagnosticDetails_ = std::move(diagnosticDetails);
    return result;
}

ToolStatus ToolResult::status() const noexcept {
    return status_;
}

const std::string& ToolResult::output() const noexcept {
    return output_;
}

ToolErrorCode ToolResult::errorCode() const noexcept {
    return errorCode_;
}

const std::string& ToolResult::errorMessage() const noexcept {
    return errorMessage_;
}

const std::string& ToolResult::diagnosticDetails() const noexcept {
    return diagnosticDetails_;
}

const ToolExecutionMetadata& ToolResult::execution() const noexcept {
    return execution_;
}

void ToolResult::attachExecutionMetadata(
    ToolExecutionMetadata metadata
) noexcept {
    execution_ = std::move(metadata);
}

std::string_view toolStatusName(ToolStatus status) noexcept {
    switch (status) {
    case ToolStatus::Succeeded: return "succeeded";
    case ToolStatus::Failed: return "failed";
    case ToolStatus::Rejected: return "rejected";
    case ToolStatus::TimedOut: return "timed_out";
    case ToolStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

std::string_view toolErrorCodeName(ToolErrorCode errorCode) noexcept {
    switch (errorCode) {
    case ToolErrorCode::None: return "none";
    case ToolErrorCode::MalformedRequest: return "malformed_request";
    case ToolErrorCode::UnknownTool: return "unknown_tool";
    case ToolErrorCode::InvalidArguments: return "invalid_arguments";
    case ToolErrorCode::PermissionDenied: return "permission_denied";
    case ToolErrorCode::ResourceNotFound: return "resource_not_found";
    case ToolErrorCode::ResourceUnavailable: return "resource_unavailable";
    case ToolErrorCode::ResourceLimitExceeded: return "resource_limit_exceeded";
    case ToolErrorCode::IoError: return "io_error";
    case ToolErrorCode::LaunchFailed: return "launch_failed";
    case ToolErrorCode::ExecutionFailed: return "execution_failed";
    case ToolErrorCode::Timeout: return "timeout";
    case ToolErrorCode::Cancelled: return "cancelled";
    case ToolErrorCode::InternalError: return "internal_error";
    }
    return "unknown";
}

} // namespace numerick