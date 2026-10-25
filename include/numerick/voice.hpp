#pragma once

#include <string>

namespace numerick {

// Capture one microphone utterance and transcribe it locally.
// Press Enter in the console to stop recording.
std::string captureAndTranscribeOnce();

// Speak UTF-8 text through the currently installed Windows SAPI voice.
void speakText(const std::string& text);

} // namespace numerick