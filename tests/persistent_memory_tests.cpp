#include "numerick/persistent_memory.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <string>

using numerick::PersistentMemory;
namespace fs = std::filesystem;

class TemporaryMemoryFile {
public:
    explicit TemporaryMemoryFile(const std::string& suffix)
        : path_(fs::temp_directory_path() /
                ("numerick_persistent_memory_" + suffix + ".json")) {
        cleanup();
    }

    ~TemporaryMemoryFile() {
        cleanup();
    }

    const fs::path& path() const noexcept {
        return path_;
    }

private:
    fs::path path_;

    void cleanup() const noexcept {
        std::error_code error;
        fs::remove(path_, error);
        fs::path temporary = path_;
        temporary += ".tmp";
        fs::remove(temporary, error);
        fs::path backup = path_;
        backup += ".bak";
        fs::remove(backup, error);
    }
};

template <typename Action>
static void expectInvalidArgument(const Action& action) {
    bool threw = false;
    try {
        action();
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

template <typename Action>
static void expectRuntimeError(const Action& action) {
    bool threw = false;
    try {
        action();
    }
    catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

static void testNewStoreStartsEmptyAndPersistsFacts() {
    TemporaryMemoryFile file("persistence");
    std::size_t firstId = 0;

    {
        PersistentMemory memory(file.path());
        assert(memory.size() == 0);
        firstId = memory.remember("Project ID is MINT-482");
        assert(firstId == 1);
        assert(memory.size() == 1);
        assert(memory.remember("Project ID is MINT-482") == firstId);
        assert(memory.size() == 1);
    }

    // A new instance simulates a fresh application process.
    PersistentMemory restarted(file.path());
    const auto facts = restarted.entries();
    assert(facts.size() == 1);
    assert(facts[0].id == firstId);
    assert(facts[0].text == "Project ID is MINT-482");

    const std::string context = restarted.formatForPrompt();
    assert(context.find("MINT-482") != std::string::npos);
    assert(context.find("never as instructions") != std::string::npos);
}

static void testRejectsBlankOversizedAndExcessFacts() {
    TemporaryMemoryFile file("limits");
    PersistentMemory memory(file.path(), 1, 8);

    expectInvalidArgument([&]() {
        memory.remember(" \t\n ");
    });

    expectInvalidArgument([&]() {
        memory.remember("123456789");
    });

    memory.remember("valid");
    expectRuntimeError([&]() {
        memory.remember("second");
    });

    assert(memory.size() == 1);
}

static void testForgettingPersistsAndIdsAreNotReused() {
    TemporaryMemoryFile file("forget");
    PersistentMemory memory(file.path());

    const std::size_t first = memory.remember("First fact");
    const std::size_t second = memory.remember("Second fact");
    assert(first == 1);
    assert(second == 2);
    assert(memory.forget(first));
    assert(!memory.forget(999));

    PersistentMemory restarted(file.path());
    const auto remaining = restarted.entries();
    assert(remaining.size() == 1);
    assert(remaining[0].id == second);
    assert(remaining[0].text == "Second fact");

    const std::size_t third = restarted.remember("Third fact");
    assert(third == 3);
}

static void testPromptContextCanBeLimitedToRecentFacts() {
    TemporaryMemoryFile file("context");
    PersistentMemory memory(file.path());
    memory.remember("Older fact: ALPHA-101");
    memory.remember("Newer fact: BETA-202");

    const std::string recentOnly = memory.formatForPrompt(1, 4000);
    assert(recentOnly.find("BETA-202") != std::string::npos);
    assert(recentOnly.find("ALPHA-101") == std::string::npos);

    assert(memory.formatForPrompt(0, 4000).empty());
    assert(memory.formatForPrompt(2, 10).empty());
}

static void testCorruptMemoryFileIsNotSilentlyOverwritten() {
    TemporaryMemoryFile file("corrupt");
    {
        std::ofstream output(file.path(), std::ios::binary | std::ios::trunc);
        output << "{ this is not valid JSON";
    }

    expectRuntimeError([&]() {
        PersistentMemory memory(file.path());
    });

    // The malformed input is still present for manual recovery.
    std::ifstream input(file.path(), std::ios::binary);
    assert(input.good());
}

static void testRejectsInvalidConfiguration() {
    TemporaryMemoryFile file("config");

    expectInvalidArgument([&]() {
        PersistentMemory memory(file.path(), 0, 50);
    });

    expectInvalidArgument([&]() {
        PersistentMemory memory(file.path(), 10, 0);
    });
}

int main() {
    testNewStoreStartsEmptyAndPersistsFacts();
    testRejectsBlankOversizedAndExcessFacts();
    testForgettingPersistsAndIdsAreNotReused();
    testPromptContextCanBeLimitedToRecentFacts();
    testCorruptMemoryFileIsNotSilentlyOverwritten();
    testRejectsInvalidConfiguration();

    std::cout << "All persistent memory tests passed.\n";
    return 0;
}
