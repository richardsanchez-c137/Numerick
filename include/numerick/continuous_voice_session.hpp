#pragma once

#include <memory>
#include <optional>
#include <string>

namespace numerick {

// Manages one continuous voice session.
//
// Loads Whisper and Silero VAD once and retains their contexts
// throughout the session.
//
// Returns:
//   - A transcript when speech is detected and transcribed.
//   - An empty string when no usable speech is recognized.
//   - std::nullopt when the user exits voice mode.
class ContinuousVoiceSession {
public:
    ContinuousVoiceSession();
    ~ContinuousVoiceSession();

    ContinuousVoiceSession(const ContinuousVoiceSession&) = delete;
    ContinuousVoiceSession& operator=(
        const ContinuousVoiceSession&
    ) = delete;

    std::optional<std::string> listenAndTranscribe();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace numerick