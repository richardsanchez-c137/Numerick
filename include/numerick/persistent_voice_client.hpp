#pragma once

#include <memory>
#include <string>

namespace numerick {

class PersistentVoiceClient {
public:
    PersistentVoiceClient();
    ~PersistentVoiceClient();

    PersistentVoiceClient(const PersistentVoiceClient&) = delete;
    PersistentVoiceClient& operator=(const PersistentVoiceClient&) = delete;

    void speak(const std::string& text);

    // Interrupt the active voice-service process tree.
    // Safe to call from another thread.
    void cancel() noexcept;

    void shutdown() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace numerick