#include "numerick/project_file_tools.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using numerick::Json;
using numerick::ToolRegistry;
using numerick::ToolResult;

class TemporaryWorkspace {
public:
    TemporaryWorkspace() {
        const auto suffix = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        root_ = fs::temp_directory_path() /
            ("numerick_project_tools_test_" + std::to_string(suffix));
        fs::create_directories(root_);
    }

    ~TemporaryWorkspace() {
        std::error_code error;
        fs::remove_all(root_, error);
    }

    const fs::path& root() const noexcept { return root_; }

private:
    fs::path root_;
};

static void writeText(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    assert(output.good());
}

static ToolResult call(
    const ToolRegistry& registry,
    const std::string& name,
    const Json& arguments
) {
    return registry.execute(name, arguments);
}

static void testRegistersOnlyReadOnlyProjectTools() {
    TemporaryWorkspace workspace;
    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());

    assert(registry.contains("list_project_files"));
    assert(registry.contains("read_project_file"));
    assert(!registry.contains("write_project_file"));
    assert(!registry.contains("delete_project_file"));
}

static void testDescribesWorkspaceArchitectureReadOnly() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "src" / "core" / "sample.cpp", "int sample() { return 1; }\n");
    writeText(workspace.root() / "include" / "numerick" / "sample.hpp", "#pragma once\n");
    writeText(workspace.root() / "tests" / "sample_tests.cpp", "int main() { return 0; }\n");
    writeText(workspace.root() / "build" / "ignored.txt", "generated content\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "describe_project_architecture", Json::object()
    );

    assert(result.success);
    assert(result.output.find("Workspace architecture overview") != std::string::npos);
    assert(result.output.find("src/core/sample.cpp") != std::string::npos);
    assert(result.output.find("include/numerick/sample.hpp") != std::string::npos);
    assert(result.output.find("tests/sample_tests.cpp") != std::string::npos);
    assert(result.output.find("ignored.txt") == std::string::npos);
    assert(result.output.find("not a complete call-graph analysis") != std::string::npos);

    const ToolResult unexpectedArgument = call(
        registry, "describe_project_architecture", {{"path", "."}}
    );
    assert(!unexpectedArgument.success);
    assert(unexpectedArgument.output.find("Unexpected argument: path") != std::string::npos);
}

static void testInspectCppMemberRelationshipReportsValueAndReferenceEvidence() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "include" / "numerick" / "owner.hpp",
        "class Owner {\npublic:\n    Owner();\nprivate:\n    numerick::ToolRegistry registry_;\n    Agent toolAgent_;\n};\n");
    writeText(workspace.root() / "include" / "agent.hpp",
        "class Agent {\npublic:\n    explicit Agent(const numerick::ToolRegistry& registry);\nprivate:\n    const numerick::ToolRegistry& registry_;\n};\n");
    writeText(workspace.root() / "src" / "owner.cpp", "Owner::Owner() : toolAgent_(registry_) {}\n");
    writeText(workspace.root() / "src" / "agent.cpp",
        "Agent::Agent(const numerick::ToolRegistry& registry) : registry_(registry) {}\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(registry, "inspect_cpp_member_relationship", {
        {"class_a", "Owner"}, {"member_a", "registry_"},
        {"class_b", "Agent"}, {"member_b", "registry_"}
    });

    assert(result.success);
    assert(result.output.find("include/numerick/owner.hpp") != std::string::npos);
    assert(result.output.find("by-value object member") != std::string::npos);
    assert(result.output.find("include/agent.hpp") != std::string::npos);
    assert(result.output.find("reference member") != std::string::npos);
    assert(result.output.find("src/owner.cpp") != std::string::npos);
    assert(result.output.find("src/agent.cpp") != std::string::npos);
    assert(result.output.find("Evidence-based conclusion") != std::string::npos);
}

static void testInspectCppComponentGroupsEvidence() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "include" / "numerick" / "widget.hpp",
        "class Widget {\npublic:\n    void render();\n};\n");
    writeText(workspace.root() / "src" / "widget.cpp",
        "#include \"numerick/widget.hpp\"\nvoid Widget::render() {}\n");
    writeText(workspace.root() / "src" / "usage.cpp",
        "void invokeWidget(Widget& widget) { widget.render(); }\n");
    writeText(workspace.root() / "tests" / "widget_tests.cpp",
        "void testWidgetRender() {\n    Widget widget;\n    widget.render();\n}\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "inspect_cpp_component", {{"symbol", "render"}}
    );

    assert(result.success);
    assert(result.output.find("Header / interface candidates") != std::string::npos);
    assert(result.output.find("Likely implementation candidates") != std::string::npos);
    assert(result.output.find("Likely call sites") != std::string::npos);
    assert(result.output.find("Related test references") != std::string::npos);
    assert(result.output.find("include/numerick/widget.hpp:3:") != std::string::npos);
    assert(result.output.find("src/widget.cpp:2:") != std::string::npos);
    assert(result.output.find("src/usage.cpp:1:") != std::string::npos);
    assert(result.output.find("tests/widget_tests.cpp:3:") != std::string::npos);
    assert(result.output.find("not compiler-resolved") != std::string::npos);
}

static void testListsSafeProjectEntriesAndFiltersPrivateFolders() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "README.md", "project readme\n");
    writeText(workspace.root() / "src" / "main.cpp", "int main() {}\n");
    writeText(workspace.root() / ".env", "TOKEN=not-for-reading\n");
    writeText(workspace.root() / "data" / "memory.json", "private memory\n");
    writeText(workspace.root() / "build" / "generated.txt", "generated\n");
    writeText(workspace.root() / "credentials.json", "private credentials\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "list_project_files", {{"path", "."}}
    );

    assert(result.success);
    assert(result.output.find("README.md") != std::string::npos);
    assert(result.output.find("src/") != std::string::npos);
    assert(result.output.find(".env") == std::string::npos);
    assert(result.output.find("data/") == std::string::npos);
    assert(result.output.find("build/") == std::string::npos);
    assert(result.output.find("credentials.json") == std::string::npos);
}

static void testReadsAllowlistedTextFile() {
    TemporaryWorkspace workspace;
    const std::string source = "# Project\n\nRead-only access.\n";
    writeText(workspace.root() / "docs" / "overview.md", source);

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "read_project_file", {{"path", "docs/overview.md"}}
    );

    assert(result.success);
    assert(result.output.find("docs/overview.md") != std::string::npos);
    assert(result.output.find("untrusted data") != std::string::npos);
    assert(result.output.find(source) != std::string::npos);
}

static void testBlocksTraversalAbsolutePathsAndPrivateFiles() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "README.md", "safe\n");
    writeText(workspace.root() / ".env", "API_KEY=secret\n");
    writeText(workspace.root() / "server.pem", "private key\n");
    writeText(workspace.root() / "data" / "memory.json", "saved facts\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());

    const ToolResult traversal = call(
        registry, "read_project_file", {{"path", "../README.md"}}
    );
    assert(!traversal.success);
    assert(traversal.output.find("traversal") != std::string::npos);

    const fs::path outsidePath = workspace.root().parent_path() / "outside.txt";
    const ToolResult absolute = call(
        registry, "read_project_file", {{"path", outsidePath.string()}}
    );
    assert(!absolute.success);
    assert(absolute.output.find("Absolute paths") != std::string::npos);

    for (const std::string& path : {
            std::string(".env"), std::string("server.pem"),
            std::string("data/memory.json")}) {
        const ToolResult blocked = call(
            registry, "read_project_file", {{"path", path}}
        );
        assert(!blocked.success);
    }
}

static void testBlocksNonTextAndOversizedFiles() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "large.txt", std::string(256 * 1024 + 1, 'x'));
    const fs::path binary = workspace.root() / "binary.txt";
    writeText(binary, std::string("text\0binary", 11));
    const fs::path invalidUtf8 = workspace.root() / "invalid_utf8.txt";
    writeText(invalidUtf8, std::string("bad\xC3\x28", 5));

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());

    const ToolResult large = call(
        registry, "read_project_file", {{"path", "large.txt"}}
    );
    assert(!large.success);
    assert(large.output.find("read limit") != std::string::npos);

    const ToolResult binaryResult = call(
        registry, "read_project_file", {{"path", "binary.txt"}}
    );
    assert(!binaryResult.success);
    assert(binaryResult.output.find("binary data") != std::string::npos);

    const ToolResult invalidUtf8Result = call(
        registry, "read_project_file", {{"path", "invalid_utf8.txt"}}
    );
    assert(!invalidUtf8Result.success);
    assert(invalidUtf8Result.output.find("valid UTF-8") != std::string::npos);
}

static void testCapsReturnedFileContent() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "long.txt", std::string(30 * 1024, 'a'));

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "read_project_file", {{"path", "long.txt"}}
    );

    assert(result.success);
    assert(result.output.find("Output truncated") != std::string::npos);
    assert(result.output.size() < 13 * 1024);
}

static void testRejectsDirectoriesAsFilesAndFilesAsDirectories() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "README.md", "safe\n");
    fs::create_directories(workspace.root() / "src");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());

    const ToolResult directoryAsFile = call(
        registry, "read_project_file", {{"path", "src"}}
    );
    assert(!directoryAsFile.success);

    const ToolResult fileAsDirectory = call(
        registry, "list_project_files", {{"path", "README.md"}}
    );
    assert(!fileAsDirectory.success);

    const ToolResult missing = call(
        registry, "read_project_file", {{"path", "missing.md"}}
    );
    assert(!missing.success);
}

static void testRejectsSymlinkEscapeWhenPlatformAllowsSymlinkCreation() {
    TemporaryWorkspace workspace;
    TemporaryWorkspace outside;
    writeText(outside.root() / "outside.md", "must not leak\n");

    std::error_code error;
    fs::create_directory_symlink(outside.root(), workspace.root() / "external", error);
    if (error) {
        // Symlink creation can be unavailable on Windows without developer mode
        // or the required privileges. Other path-policy tests still run.
        return;
    }

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "read_project_file", {{"path", "external/outside.md"}}
    );
    assert(!result.success);
}

static void testRegistersSearchAndSymbolTools() {
    TemporaryWorkspace workspace;
    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());

    assert(registry.contains("search_project_files"));
    assert(registry.contains("find_cpp_symbol"));
    assert(registry.contains("describe_project_architecture"));
    assert(registry.contains("inspect_cpp_component"));
    assert(registry.contains("inspect_cpp_member_relationship"));
    assert(!registry.contains("write_project_file"));
    assert(!registry.contains("run_shell_command"));
}

static void testSearchProjectFilesReturnsPathAndLineMatches() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "src" / "main.cpp",
        "#include <string>\nint main() {\n    const std::string marker = \"NeedleValue\";\n    return 0;\n}\n");
    writeText(workspace.root() / "secrets" / "credentials.json", "NeedleValue hidden\n");
    writeText(workspace.root() / "build" / "output.txt", "NeedleValue generated\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "search_project_files", {{"query", "needlevalue"}}
    );

    assert(result.success);
    assert(result.output.find("src/main.cpp:3:") != std::string::npos);
    assert(result.output.find("NeedleValue") != std::string::npos);
    assert(result.output.find("credentials.json") == std::string::npos);
    assert(result.output.find("output.txt") == std::string::npos);
}

static void testFindCppSymbolUsesWholeIdentifierBoundaries() {
    TemporaryWorkspace workspace;
    writeText(workspace.root() / "src" / "agent.cpp",
        "int runAgent();\nint runAgentFast();\nvoid caller() { runAgent(); }\n");
    writeText(workspace.root() / "docs" / "notes.md", "runAgent is discussed here.\n");

    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "find_cpp_symbol", {{"symbol", "runAgent"}}
    );

    assert(result.success);
    assert(result.output.find("src/agent.cpp:1:") != std::string::npos);
    assert(result.output.find("src/agent.cpp:3:") != std::string::npos);
    assert(result.output.find("src/agent.cpp:2:") == std::string::npos);
    assert(result.output.find("not a C++ parser") != std::string::npos);
    assert(result.output.find("notes.md") == std::string::npos);
}

static void testFindCppSymbolRejectsInvalidIdentifiers() {
    TemporaryWorkspace workspace;
    ToolRegistry registry;
    numerick::registerProjectFileTools(registry, workspace.root());
    const ToolResult result = call(
        registry, "find_cpp_symbol", {{"symbol", "../secret"}}
    );
    assert(!result.success);
    assert(result.output.find("valid C++ identifier") != std::string::npos);
}

int main() {
    testRegistersOnlyReadOnlyProjectTools();
    testRegistersSearchAndSymbolTools();
    testDescribesWorkspaceArchitectureReadOnly();
    testInspectCppComponentGroupsEvidence();
    testInspectCppMemberRelationshipReportsValueAndReferenceEvidence();
    testSearchProjectFilesReturnsPathAndLineMatches();
    testFindCppSymbolUsesWholeIdentifierBoundaries();
    testFindCppSymbolRejectsInvalidIdentifiers();
    testListsSafeProjectEntriesAndFiltersPrivateFolders();
    testReadsAllowlistedTextFile();
    testBlocksTraversalAbsolutePathsAndPrivateFiles();
    testBlocksNonTextAndOversizedFiles();
    testCapsReturnedFileContent();
    testRejectsDirectoriesAsFilesAndFilesAsDirectories();
    testRejectsSymlinkEscapeWhenPlatformAllowsSymlinkCreation();

    std::cout << "All project file tool tests passed.\n";
    return 0;
}
