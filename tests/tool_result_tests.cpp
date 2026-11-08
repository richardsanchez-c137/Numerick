#include "numerick/tool_result.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace numerick;

template <typename Function>
void expectInvalidArgument(Function&& function) {
    bool threw = false;
    try {
        function();
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

int main() {
    const auto success = ToolResult::succeeded("hello");
    assert(success.status() == ToolStatus::Succeeded);
    assert(success.errorCode() == ToolErrorCode::None);
    assert(success.output() == "hello");
    assert(success.errorMessage().empty());
    assert(success.diagnosticDetails().empty());
    assert(!success.execution().duration.has_value());
    assert(!success.execution().handlerInvoked);

    const auto failed = ToolResult::failed(
        ToolErrorCode::IoError, "Read failed", "disk diagnostic"
    );
    assert(failed.status() == ToolStatus::Failed);
    assert(failed.errorCode() == ToolErrorCode::IoError);
    assert(failed.output().empty());
    assert(failed.errorMessage() == "Read failed");
    assert(failed.diagnosticDetails() == "disk diagnostic");

    const auto rejected = ToolResult::rejected(
        ToolErrorCode::InvalidArguments, "Bad arguments"
    );
    assert(rejected.status() == ToolStatus::Rejected);
    assert(rejected.errorCode() == ToolErrorCode::InvalidArguments);
    assert(rejected.output().empty());

    const auto timedOut = ToolResult::timedOut("Deadline exceeded");
    assert(timedOut.status() == ToolStatus::TimedOut);
    assert(timedOut.errorCode() == ToolErrorCode::Timeout);
    assert(timedOut.output().empty());

    const auto cancelled = ToolResult::cancelled("Operation cancelled");
    assert(cancelled.status() == ToolStatus::Cancelled);
    assert(cancelled.errorCode() == ToolErrorCode::Cancelled);
    assert(cancelled.output().empty());

    expectInvalidArgument([] {
        (void)ToolResult::failed(ToolErrorCode::None, "Invalid");
    });
    expectInvalidArgument([] {
        (void)ToolResult::failed(ToolErrorCode::UnknownTool, "Invalid");
    });
    expectInvalidArgument([] {
        (void)ToolResult::rejected(ToolErrorCode::ExecutionFailed, "Invalid");
    });
    expectInvalidArgument([] {
        (void)ToolResult::rejected(ToolErrorCode::None, "Invalid");
    });
    expectInvalidArgument([] {
        (void)ToolResult::failed(ToolErrorCode::IoError, "");
    });
    expectInvalidArgument([] {
        (void)ToolResult::rejected(ToolErrorCode::InvalidArguments, "");
    });
    expectInvalidArgument([] {
        (void)ToolResult::timedOut("");
    });
    expectInvalidArgument([] {
        (void)ToolResult::cancelled("");
    });

    assert(toolStatusName(ToolStatus::Succeeded) == "succeeded");
    assert(toolStatusName(ToolStatus::Failed) == "failed");
    assert(toolStatusName(ToolStatus::Rejected) == "rejected");
    assert(toolStatusName(ToolStatus::TimedOut) == "timed_out");
    assert(toolStatusName(ToolStatus::Cancelled) == "cancelled");

    assert(toolErrorCodeName(ToolErrorCode::None) == "none");
    assert(toolErrorCodeName(ToolErrorCode::InvalidArguments) == "invalid_arguments");
    assert(toolErrorCodeName(ToolErrorCode::PermissionDenied) == "permission_denied");
    assert(toolErrorCodeName(ToolErrorCode::InternalError) == "internal_error");

    std::cout << "All tool result tests passed.\n";
    return 0;
}