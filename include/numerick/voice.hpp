#pragma once

#include <string>

namespace numerick {

// Capture one microphone utterance and transcribe it locally.
std::string captureAndTranscribeOnce();

// Speak UTF-8 text through the configured speech engine.
void speakText(const std::string& text);

// Cancel currently active speech playback or synthesis.
// Safe to call from another thread.
void cancelSpeech() noexcept;

} // namespace numerick