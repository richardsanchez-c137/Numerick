#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace numerick {

struct PersistentMemoryEntry {
    std::size_t id = 0;
    std::string text;
};

// Stores facts explicitly saved by the user in a local JSON file.
// Conversation history is deliberately managed by ConversationSession.
class PersistentMemory {
public:
    explicit PersistentMemory(
        std::filesystem::path filePath,
        std::size_t maxEntries = 50,
        std::size_t maxFactLength = 500
    );

    // Saves a fact and returns its stable ID. Re-saving an exact duplicate
    // returns the existing ID instead of creating a duplicate record.
    std::size_t remember(const std::string& fact);

    std::vector<PersistentMemoryEntry> entries() const;

    // Returns true only when an entry existed and was removed.
    bool forget(std::size_t id);

    // Formats the newest saved facts as bounded JSON context for the model.
    // The returned text explicitly marks stored values as data, not instructions.
    std::string formatForPrompt(
        std::size_t maxEntries = 20,
        std::size_t maxCharacters = 4000
    ) const;

    std::size_t size() const noexcept;
    std::size_t maxEntries() const noexcept;

private:
    std::filesystem::path filePath_;
    std::size_t maxEntries_;
    std::size_t maxFactLength_;
    std::size_t nextId_ = 1;
    std::vector<PersistentMemoryEntry> entries_;

    void load();
    void save() const;
};

} // namespace numerick
