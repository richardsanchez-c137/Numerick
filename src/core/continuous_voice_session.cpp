
#include "numerick/continuous_voice_session.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define SDL_MAIN_HANDLED

#include <windows.h>
#include <conio.h>
#include <SDL.h>
#include <whisper.h>
#include <ggml.h>
#include <ggml-backend.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <cctype>

namespace numerick {
namespace {

constexpr int kSampleRate = 16000;
constexpr std::size_t kVadFrameSamples = 512;

constexpr std::size_t kRingCapacity =
    static_cast<std::size_t>(kSampleRate) * 10;

constexpr std::size_t kPreRollSamples =
    static_cast<std::size_t>(kSampleRate) * 320 / 1000;

constexpr std::size_t kMaxUtteranceSamples =
    static_cast<std::size_t>(kSampleRate) * 25;

constexpr std::size_t kMinimumUtteranceSamples =
    static_cast<std::size_t>(kSampleRate) * 300 / 1000;

constexpr int kSpeechStartWindows = 3;
constexpr float kSpeechThreshold = 0.5F;

constexpr auto kEndOfSpeechSilence =
    std::chrono::milliseconds(850);

std::once_flag gBackendLoadFlag;

std::string environmentOrDefault(
    const char* name,
    const char* fallback
) {
    const char* value = std::getenv(name);

    if (value != nullptr && value[0] != '\0') {
        return value;
    }

    return fallback;
}

void quietWhisperLog(
    enum ggml_log_level level,
    const char* message,
    void*
) {
    if ((level == GGML_LOG_LEVEL_ERROR ||
         level == GGML_LOG_LEVEL_WARN) &&
        message != nullptr) {
        std::cerr << message;
    }
}

struct WhisperDeleter {
    void operator()(whisper_context* context) const noexcept {
        if (context != nullptr) {
            whisper_free(context);
        }
    }
};

std::string trimText(std::string value) {
    const auto first =
        value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return {};
    }

    const auto last =
        value.find_last_not_of(" \t\r\n");

    return value.substr(0, last - first + 1);
}

std::string lowercaseAscii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
        );
    }

    return value;
}

bool isWordCharacter(char character) {
    return std::isalnum(
        static_cast<unsigned char>(character)
    ) != 0 || character == '_';
}

bool startsWithPhrase(
    const std::string& text,
    const std::string& phrase
) {
    if (text.compare(0, phrase.size(), phrase) != 0) {
        return false;
    }

    return text.size() == phrase.size() ||
        !isWordCharacter(text[phrase.size()]);
}

void replaceWholePhraseCaseInsensitive(
    std::string& text,
    const std::string& target,
    const std::string& replacement
) {
    const std::string targetLower =
        lowercaseAscii(target);

    std::string textLower =
        lowercaseAscii(text);

    std::size_t position = 0;

    while ((position = textLower.find(
        targetLower,
        position
    )) != std::string::npos) {

        const bool leftBoundary =
            position == 0 ||
            !isWordCharacter(text[position - 1]);

        const std::size_t end =
            position + target.size();

        const bool rightBoundary =
            end >= text.size() ||
            !isWordCharacter(text[end]);

        if (leftBoundary && rightBoundary) {
            text.replace(
                position,
                target.size(),
                replacement
            );

            textLower = lowercaseAscii(text);

            position += replacement.size();
        }
        else {
            position += targetLower.size();
        }
    }
}

std::string normalizeProjectVocabulary(
    std::string transcript
) {
    const std::string lower =
        lowercaseAscii(transcript);

    const bool greeting =
        startsWithPhrase(lower, "hello") ||
        startsWithPhrase(lower, "hi") ||
        startsWithPhrase(lower, "hey") ||
        startsWithPhrase(lower, "good morning") ||
        startsWithPhrase(lower, "good afternoon") ||
        startsWithPhrase(lower, "good evening");

    const bool assistantNameContext =
        greeting ||
        lower == "numeric" ||
        lower.find("called numeric") != std::string::npos ||
        lower.find("named numeric") != std::string::npos ||
        startsWithPhrase(lower, "numeric uses") ||
        startsWithPhrase(lower, "numeric is");

    // Correct the assistant's name only in likely name contexts.
    if (assistantNameContext) {
        replaceWholePhraseCaseInsensitive(
            transcript,
            "numeric",
            "NUMERICK"
        );

        replaceWholePhraseCaseInsensitive(
            transcript,
            "new merik",
            "NUMERICK"
        );

        replaceWholePhraseCaseInsensitive(
            transcript,
            "new medic",
            "NUMERICK"
        );
    }

    // Always normalize the official capitalization.
    replaceWholePhraseCaseInsensitive(
        transcript,
        "numerick",
        "NUMERICK"
    );

    // Correct the known Ollama transcription error.
    replaceWholePhraseCaseInsensitive(
        transcript,
        "olama",
        "Ollama"
    );

    return transcript;
}

} // namespace

class ContinuousVoiceSession::Impl {
public:
    Impl()
        : ringBuffer_(kRingCapacity, 0.0F) {
        try {
            initialize();
        }
        catch (...) {
            cleanup();
            throw;
        }
    }

    ~Impl() {
        cleanup();
    }

    void cleanup() noexcept {
        if (audioDevice_ != 0) {
            SDL_PauseAudioDevice(audioDevice_, 1);
            SDL_CloseAudioDevice(audioDevice_);
            audioDevice_ = 0;
        }

        if (audioInitialized_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audioInitialized_ = false;
        }

        if (vad_ != nullptr) {
            whisper_vad_free(vad_);
            vad_ = nullptr;
        }

        if (whisper_ != nullptr) {
            whisper_free(whisper_);
            whisper_ = nullptr;
        }

        if (logCallbackInstalled_) {
            whisper_log_set(nullptr, nullptr);
            logCallbackInstalled_ = false;
        }
    }

    void initialize() {
        // Load dynamically registered GGML backends once.
        std::call_once(gBackendLoadFlag, [] {
            ggml_backend_load_all();
        });

        // Suppress repetitive per-frame VAD diagnostic messages.
        whisper_log_set(quietWhisperLog, nullptr);
        logCallbackInstalled_ = true;

        const std::string whisperModel =
            environmentOrDefault(
                "NUMERICK_WHISPER_MODEL",
                "models/ggml-base.en.bin"
            );

        const std::string vadModel =
            environmentOrDefault(
                "NUMERICK_VAD_MODEL",
                "models/ggml-silero-v6.2.0.bin"
            );

        if (!std::filesystem::is_regular_file(whisperModel)) {
            throw std::runtime_error(
                "Whisper model not found: " + whisperModel
            );
        }

        if (!std::filesystem::is_regular_file(vadModel)) {
            throw std::runtime_error(
                "Silero VAD model not found: " + vadModel
            );
        }

        std::cout
            << "NUMERICK > Loading continuous voice models...\n";

        auto whisperParams =
            whisper_context_default_params();

        whisperParams.use_gpu = false;

        whisper_ = whisper_init_from_file_with_params(
            whisperModel.c_str(),
            whisperParams
        );

        if (whisper_ == nullptr) {
            throw std::runtime_error(
                "Could not initialize the Whisper model."
            );
        }

        auto vadParams =
            whisper_vad_default_context_params();

        vadParams.n_threads = 4;
        vadParams.use_gpu = false;

        vad_ = whisper_vad_init_from_file_with_params(
            vadModel.c_str(),
            vadParams
        );

        if (vad_ == nullptr) {
            throw std::runtime_error(
                "Could not initialize the Silero VAD model."
            );
        }

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            throw std::runtime_error(
                std::string("Could not initialize audio: ") +
                SDL_GetError()
            );
        }

        audioInitialized_ = true;

        SDL_AudioSpec desired{};
        SDL_AudioSpec obtained{};

        desired.freq = kSampleRate;
        desired.format = AUDIO_F32SYS;
        desired.channels = 1;
        desired.samples =
            static_cast<Uint16>(kVadFrameSamples);
        desired.callback = audioCallback;
        desired.userdata = this;

        audioDevice_ = SDL_OpenAudioDevice(
            nullptr,
            SDL_TRUE,
            &desired,
            &obtained,
            0
        );

        if (audioDevice_ == 0) {
            throw std::runtime_error(
                std::string("Could not open microphone: ") +
                SDL_GetError()
            );
        }

        if (obtained.freq != kSampleRate ||
            obtained.format != AUDIO_F32SYS ||
            obtained.channels != 1) {
            throw std::runtime_error(
                "Microphone must support 16 kHz, mono, "
                "32-bit floating-point capture."
            );
        }

        std::cout
            << "NUMERICK > Whisper and Silero VAD are ready.\n";
    }

    // Called by SDL's microphone capture thread.
    static void SDLCALL audioCallback(
        void* userdata,
        Uint8* stream,
        int length
    ) {
        if (userdata == nullptr ||
            stream == nullptr ||
            length <= 0) {
            return;
        }

        auto* self =
            static_cast<Impl*>(userdata);

        if (length % static_cast<int>(sizeof(float)) != 0) {
            self->invalidAudioFormat_.store(true);
            return;
        }

        const auto* input =
            reinterpret_cast<const float*>(stream);

        const std::size_t count =
            static_cast<std::size_t>(length) / sizeof(float);

        const std::uint64_t write =
            self->writeCursor_.load(
                std::memory_order_relaxed
            );

        const std::uint64_t read =
            self->readCursor_.load(
                std::memory_order_acquire
            );

        const std::uint64_t queued = write - read;

        // Never block the real-time audio callback.
        if (queued + count > kRingCapacity) {
            self->bufferOverflow_.store(true);
            return;
        }

        for (std::size_t i = 0; i < count; ++i) {
            self->ringBuffer_[
                (write + i) % kRingCapacity
            ] = input[i];
        }

        self->writeCursor_.store(
            write + count,
            std::memory_order_release
        );
    }

    bool popVadFrame(
        std::array<float, kVadFrameSamples>& frame
    ) {
        const std::uint64_t read =
            readCursor_.load(std::memory_order_relaxed);

        const std::uint64_t write =
            writeCursor_.load(std::memory_order_acquire);

        if (write - read < kVadFrameSamples) {
            return false;
        }

        for (std::size_t i = 0;
             i < kVadFrameSamples;
             ++i) {
            frame[i] =
                ringBuffer_[
                    (read + i) % kRingCapacity
                ];
        }

        readCursor_.store(
            read + kVadFrameSamples,
            std::memory_order_release
        );

        return true;
    }

    std::optional<std::string> listenAndTranscribe() {
        if (audioDevice_ == 0 ||
            whisper_ == nullptr ||
            vad_ == nullptr) {
            throw std::runtime_error(
                "Continuous voice session is not initialized."
            );
        }

        // Clear leftover audio from the previous turn.
        const auto latest =
            writeCursor_.load(std::memory_order_acquire);

        readCursor_.store(
            latest,
            std::memory_order_release
        );

        bufferOverflow_.store(false);
        invalidAudioFormat_.store(false);

        whisper_vad_reset_state(vad_);

        std::deque<float> preRoll;
        std::vector<float> utterance;

        bool recording = false;
        int consecutiveSpeechWindows = 0;

        auto lastSpeechTime =
            std::chrono::steady_clock::now();

        std::array<float, kVadFrameSamples> frame{};

        std::cout
            << "\nNUMERICK > Listening... "
            << "(Press ESC to end voice mode)\n";

        SDL_PauseAudioDevice(audioDevice_, 0);

        try {
            while (true) {
                // ESC ends the continuous session.
                if (_kbhit()) {
                    const int key = _getch();

                    if (key == 27) {
                        SDL_PauseAudioDevice(
                            audioDevice_,
                            1
                        );

                        std::cout
                            << "\nNUMERICK > Voice session stopped.\n";

                        return std::nullopt;
                    }
                }

                if (invalidAudioFormat_.load()) {
                    throw std::runtime_error(
                        "Microphone returned an invalid audio format."
                    );
                }

                // Recover cleanly if the audio queue ever overflows.
                if (bufferOverflow_.exchange(false)) {
                    const auto newest =
                        writeCursor_.load(std::memory_order_acquire);

                    readCursor_.store(
                        newest,
                        std::memory_order_release
                    );

                    whisper_vad_reset_state(vad_);

                    preRoll.clear();
                    utterance.clear();

                    recording = false;
                    consecutiveSpeechWindows = 0;

                    std::cerr
                        << "[Voice] Microphone buffer overflow; "
                        << "resuming listening.\n";

                    continue;
                }

                if (!popVadFrame(frame)) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(4)
                    );

                    continue;
                }

                // Process exactly one 512-sample VAD window.
                if (!whisper_vad_detect_speech_no_reset(
                        vad_,
                        frame.data(),
                        static_cast<int>(frame.size())
                    )) {
                    throw std::runtime_error(
                        "Silero VAD inference failed."
                    );
                }

                const int probabilityCount =
                    whisper_vad_n_probs(vad_);

                float probability = 0.0F;

                if (probabilityCount > 0) {
                    float* probabilities =
                        whisper_vad_probs(vad_);

                    if (probabilities != nullptr) {
                        probability =
                            probabilities[probabilityCount - 1];
                    }
                }

                const bool speechDetected =
                    probability >= kSpeechThreshold;

                const auto now =
                    std::chrono::steady_clock::now();

                if (!recording) {
                    // Preserve audio immediately before speech starts.
                    preRoll.insert(
                        preRoll.end(),
                        frame.begin(),
                        frame.end()
                    );

                    while (preRoll.size() > kPreRollSamples) {
                        preRoll.pop_front();
                    }

                    if (speechDetected) {
                        ++consecutiveSpeechWindows;
                    }
                    else {
                        consecutiveSpeechWindows = 0;
                    }

                    // Require approximately 96 ms of detected speech.
                    if (consecutiveSpeechWindows >=
                        kSpeechStartWindows) {
                        recording = true;

                        utterance.assign(
                            preRoll.begin(),
                            preRoll.end()
                        );

                        lastSpeechTime = now;

                        std::cout
                            << "\nNUMERICK > Speech detected. "
                            << "Listening...\n";
                    }

                    continue;
                }

                // Continue collecting the active utterance.
                utterance.insert(
                    utterance.end(),
                    frame.begin(),
                    frame.end()
                );

                if (speechDetected) {
                    lastSpeechTime = now;
                }

                const auto silenceDuration =
                    std::chrono::duration_cast<
                        std::chrono::milliseconds
                    >(now - lastSpeechTime);

                const bool silenceEnded =
                    silenceDuration >= kEndOfSpeechSilence;

                const bool maximumReached =
                    utterance.size() >= kMaxUtteranceSamples;

                if (silenceEnded || maximumReached) {
                    break;
                }
            }

            // Do not listen while the application thinks or speaks.
            SDL_PauseAudioDevice(audioDevice_, 1);
        }
        catch (...) {
            SDL_PauseAudioDevice(audioDevice_, 1);
            throw;
        }

        whisper_vad_reset_state(vad_);

        if (utterance.size() < kMinimumUtteranceSamples) {
            std::cout
                << "NUMERICK > Speech was too short. Try again.\n";

            return std::string{};
        }

        return transcribe(utterance);
    }

    std::string transcribe(
        const std::vector<float>& samples
    ) {
        auto params =
            whisper_full_default_params(
                WHISPER_SAMPLING_GREEDY
            );

        const unsigned hardwareThreads =
            std::max(
                2U,
                std::thread::hardware_concurrency()
            );

        params.n_threads = static_cast<int>(
            std::min(8U, hardwareThreads)
        );

        const std::string language =
            environmentOrDefault(
                "NUMERICK_WHISPER_LANGUAGE",
                "en"
            );
            params.language = language.c_str();
            params.detect_language = (language == "auto");

            // Vocabulary context for NUMERICK-specific terminology.
            // This biases Whisper toward these spellings when acoustically plausible.
            const std::string vocabularyPrompt =
                "This is a conversation with NUMERICK, "
                "a personal AI software assistant. "
                "NUMERICK is the assistant's name. "
                "Technical vocabulary: Ollama, Qwen, Qwen3.5, "
                "NVIDIA, NVIGI, Silero, Whisper, GGML, CUDA, "
                "MSYS2, C++.";

            params.initial_prompt = vocabularyPrompt.c_str();

            // Reapply vocabulary context to every decoding window.
            params.carry_initial_prompt = true;

            // Keep separate user utterances independent.
            params.no_context = true;

        params.print_progress = false;
        params.print_realtime = false;
        params.print_timestamps = false;

        std::cout
            << "NUMERICK > Transcribing...\n";

        const int result = whisper_full(
            whisper_,
            params,
            samples.data(),
            static_cast<int>(samples.size())
        );

        if (result != 0) {
            throw std::runtime_error(
                "Whisper failed to transcribe the utterance."
            );
        }

        std::string transcript;

        const int segmentCount =
            whisper_full_n_segments(whisper_);

        for (int i = 0; i < segmentCount; ++i) {
            const char* segment =
                whisper_full_get_segment_text(
                    whisper_,
                    i
                );

            if (segment != nullptr) {
                transcript += segment;
            }
        }

        return normalizeProjectVocabulary(
            trimText(std::move(transcript))
        );
    }

private:
    whisper_context* whisper_ = nullptr;
    whisper_vad_context* vad_ = nullptr;

    SDL_AudioDeviceID audioDevice_ = 0;
    bool audioInitialized_ = false;
    bool logCallbackInstalled_ = false;

    std::vector<float> ringBuffer_;

    std::atomic<std::uint64_t> writeCursor_{0};
    std::atomic<std::uint64_t> readCursor_{0};

    std::atomic<bool> bufferOverflow_{false};
    std::atomic<bool> invalidAudioFormat_{false};
};

ContinuousVoiceSession::ContinuousVoiceSession()
    : impl_(std::make_unique<Impl>()) {}

ContinuousVoiceSession::~ContinuousVoiceSession() = default;

std::optional<std::string>
ContinuousVoiceSession::listenAndTranscribe() {
    return impl_->listenAndTranscribe();
}

} // namespace numerick