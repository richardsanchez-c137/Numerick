#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define SDL_MAIN_HANDLED
#include <windows.h>
#include <mmsystem.h>
#include <sapi.h>
#include <SDL.h>
#include <whisper.h>
#include "numerick/voice.hpp"
#include <ggml-backend.h>
#include "numerick/persistent_voice_client.hpp"
#include <mutex>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>



namespace numerick {
namespace {

constexpr int kSampleRate = 16000;
constexpr DWORD kTtsSynthesisTimeoutMs = 180000; // 3 minutes
constexpr DWORD kTtsTerminationWaitMs = 10000;   // 10 seconds
std::once_flag ggmlBackendLoadOnce;
constexpr std::size_t kMaxRecordingSeconds = 30;
constexpr std::size_t kMaxSamples =
    static_cast<std::size_t>(kSampleRate) * kMaxRecordingSeconds;

struct AudioCapture {
    // Allocate the bounded buffer before audio starts. The audio callback
    // writes into it without allocating memory or taking a mutex.
    std::vector<float> samples =
        std::vector<float>(kMaxSamples, 0.0F);

    std::size_t written = 0;
    bool truncated = false;
    bool invalidFormat = false;
};

void SDLCALL captureCallback(
    void* userdata,
    Uint8* stream,
    int length
) {
    if (userdata == nullptr || stream == nullptr || length <= 0) {
        return;
    }

    auto& capture = *static_cast<AudioCapture*>(userdata);

    if (length % static_cast<int>(sizeof(float)) != 0) {
        capture.invalidFormat = true;
        return;
    }

    const auto* input = reinterpret_cast<const float*>(stream);
    const std::size_t count =
        static_cast<std::size_t>(length) / sizeof(float);

    const std::size_t remaining = kMaxSamples - capture.written;
    const std::size_t toCopy = std::min(count, remaining);

    std::copy_n(
        input,
        toCopy,
        capture.samples.begin() +
            static_cast<std::ptrdiff_t>(capture.written)
    );

    capture.written += toCopy;

    if (toCopy < count) {
        capture.truncated = true;
    }
}

struct WhisperContextDeleter {
    void operator()(whisper_context* context) const noexcept {
        if (context != nullptr) {
            whisper_free(context);
        }
    }
};

struct AudioDevice {
    SDL_AudioDeviceID id = 0;

    ~AudioDevice() {
        if (id != 0) {
            SDL_CloseAudioDevice(id);
        }
    }

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    AudioDevice() = default;
};

struct AudioSubsystem {
    AudioSubsystem() {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            throw std::runtime_error(
                std::string("Could not initialize microphone audio: ") +
                SDL_GetError()
            );
        }
    }

    ~AudioSubsystem() {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    AudioSubsystem(const AudioSubsystem&) = delete;
    AudioSubsystem& operator=(const AudioSubsystem&) = delete;
};

struct ComApartment {
    ComApartment() {
        const HRESULT result =
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        if (FAILED(result)) {
            throw std::runtime_error(
                "Could not initialize Windows speech services. HRESULT=" +
                std::to_string(static_cast<unsigned long>(result))
            );
        }
    }

    ~ComApartment() {
        CoUninitialize();
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};

struct SpVoiceDeleter {
    void operator()(ISpVoice* voice) const noexcept {
        if (voice != nullptr) {
            voice->Release();
        }
    }
};

std::string configuredModelPath() {
    const char* value = std::getenv("NUMERICK_WHISPER_MODEL");

    if (value != nullptr && value[0] != '\0') {
        return value;
    }

    return "models/ggml-base.en.bin";
}

std::string configuredLanguage() {
    const char* value = std::getenv("NUMERICK_WHISPER_LANGUAGE");

    if (value != nullptr && value[0] != '\0') {
        return value;
    }

    return "en";
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(0, last - first + 1);
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    if (text.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Speech text is too large.");
    }

    const int inputLength = static_cast<int>(text.size());

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        inputLength,
        nullptr,
        0
    );

    if (required <= 0) {
        throw std::runtime_error(
            "Could not convert UTF-8 text for Windows speech output."
        );
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            inputLength,
            result.data(),
            required
        ) != required) {
        throw std::runtime_error(
            "Could not convert UTF-8 text for Windows speech output."
        );
    }

    return result;
}

std::wstring readWideEnvironmentVariable(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);

    if (required == 0) {
        return {};
    }

    std::wstring value(static_cast<std::size_t>(required), L'\0');

    const DWORD written = GetEnvironmentVariableW(
        name,
        value.data(),
        required
    );

    if (written == 0 || written >= required) {
        return {};
    }

    value.resize(static_cast<std::size_t>(written));
    return value;
}

std::wstring quoteWindowsArgument(const std::wstring& value) {
    std::wstring result = L"\"";
    std::size_t backslashes = 0;

    for (const wchar_t character : value) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }

        if (character == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        }
        else {
            result.append(backslashes, L'\\');
            result.push_back(character);
        }

        backslashes = 0;
    }

    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');

    return result;
}

bool speakWithNvidiaQwen(const std::string& text) {
    const std::wstring serviceValue =
        readWideEnvironmentVariable(L"NUMERICK_VOICE_SERVICE_EXE");

    const std::wstring outputValue =
        readWideEnvironmentVariable(L"NUMERICK_TTS_OUTPUT_DIR");

    // No custom voice configuration: let the caller use SAPI.
    if (serviceValue.empty() || outputValue.empty()) {
        return false;
    }

    const std::filesystem::path servicePath(serviceValue);
    const std::filesystem::path outputDirectory(outputValue);

    if (!std::filesystem::is_regular_file(servicePath)) {
        throw std::runtime_error(
            "The Rust voice-service executable was not found."
        );
    }

    std::error_code directoryError;
    std::filesystem::create_directories(
        outputDirectory,
        directoryError
    );

    if (directoryError ||
        !std::filesystem::is_directory(outputDirectory)) {
        throw std::runtime_error(
            "Could not access the custom voice output directory."
        );
    }

    // Unique, filesystem-safe output name.
    const std::wstring outputName =
        L"numerick-" +
        std::to_wstring(GetCurrentProcessId()) +
        L"-" +
        std::to_wstring(GetTickCount64());

    const std::filesystem::path wavPath =
        outputDirectory / (outputName + L".wav");

    const std::filesystem::path logPath =
        outputDirectory / (outputName + L".log");

    // Keep NVIDIA/Rust diagnostics in a file instead of flooding the console.
    SECURITY_ATTRIBUTES logSecurity{};
    logSecurity.nLength = sizeof(logSecurity);
    logSecurity.bInheritHandle = TRUE;

    HANDLE logHandle = CreateFileW(
        logPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &logSecurity,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (logHandle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(
            "Could not create the voice-service diagnostic log."
        );
    }

        const std::wstring commandLine =
        quoteWindowsArgument(servicePath.wstring()) +
        L" --text " + quoteWindowsArgument(utf8ToWide(text)) +
        L" --output-name " + quoteWindowsArgument(outputName);

    std::vector<wchar_t> mutableCommandLine(
        commandLine.begin(),
        commandLine.end()
    );
    mutableCommandLine.push_back(L'\0');

    // Place the Rust worker and its NVIDIA child process in one job.
    // Closing the job terminates every process belonging to it.
    HANDLE jobHandle = CreateJobObjectW(nullptr, nullptr);

    if (jobHandle == nullptr) {
        const DWORD error = GetLastError();
        CloseHandle(logHandle);

        throw std::runtime_error(
            "Could not create the voice-service process job. Windows error: " +
            std::to_string(error)
        );
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimits{};
    jobLimits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(
            jobHandle,
            JobObjectExtendedLimitInformation,
            &jobLimits,
            sizeof(jobLimits)
        )) {
        const DWORD error = GetLastError();
        CloseHandle(logHandle);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Could not configure voice-service process cleanup. Windows error: " +
            std::to_string(error)
        );
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = logHandle;
    startupInfo.hStdOutput = logHandle;
    startupInfo.hStdError = logHandle;

    PROCESS_INFORMATION processInfo{};

    const std::filesystem::path serviceDirectory =
        servicePath.parent_path();

    // Start suspended so the process can be assigned to its job
    // before it has an opportunity to spawn NVIDIA.
    const BOOL started = CreateProcessW(
        servicePath.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr,
        serviceDirectory.c_str(),
        &startupInfo,
        &processInfo
    );

    const DWORD launchError =
        started ? ERROR_SUCCESS : GetLastError();

    CloseHandle(logHandle);

    if (!started) {
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Could not start the Rust voice service. Windows error: " +
            std::to_string(launchError)
        );
    }

    if (!AssignProcessToJobObject(
            jobHandle,
            processInfo.hProcess
        )) {
        const DWORD error = GetLastError();

        TerminateProcess(processInfo.hProcess, 1);
        WaitForSingleObject(
            processInfo.hProcess,
            kTtsTerminationWaitMs
        );

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Could not assign the Rust voice service to its process job. "
            "Windows error: " + std::to_string(error)
        );
    }

    if (ResumeThread(processInfo.hThread) ==
        static_cast<DWORD>(-1)) {
        const DWORD error = GetLastError();

        TerminateJobObject(jobHandle, 1);
        WaitForSingleObject(
            processInfo.hProcess,
            kTtsTerminationWaitMs
        );

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Could not start the Rust voice-service thread. Windows error: " +
            std::to_string(error)
        );
    }

    CloseHandle(processInfo.hThread);

    const DWORD waitResult = WaitForSingleObject(
        processInfo.hProcess,
        kTtsSynthesisTimeoutMs
    );

    if (waitResult == WAIT_TIMEOUT) {
        TerminateJobObject(jobHandle, ERROR_TIMEOUT);

        WaitForSingleObject(
            processInfo.hProcess,
            kTtsTerminationWaitMs
        );

        CloseHandle(processInfo.hProcess);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Voice synthesis exceeded the 180-second timeout. "
            "The Rust worker and its child processes were terminated."
        );
    }

    if (waitResult != WAIT_OBJECT_0) {
        const DWORD error = GetLastError();

        TerminateJobObject(jobHandle, 1);

        WaitForSingleObject(
            processInfo.hProcess,
            kTtsTerminationWaitMs
        );

        CloseHandle(processInfo.hProcess);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Waiting for the Rust voice service failed. Windows error: " +
            std::to_string(error)
        );
    }

    DWORD exitCode = 1;

    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        const DWORD error = GetLastError();

        CloseHandle(processInfo.hProcess);
        CloseHandle(jobHandle);

        throw std::runtime_error(
            "Could not read the voice-service exit code. Windows error: " +
            std::to_string(error)
        );
    }

    CloseHandle(processInfo.hProcess);

    // Also terminates any unexpected leftover child processes.
    CloseHandle(jobHandle);

    if (exitCode != 0) {
        throw std::runtime_error(
            "The Rust voice service failed with exit code " +
            std::to_string(exitCode) +
            ". Its diagnostic log was retained in the generated folder."
        );
    }

    std::error_code fileError;

    if (!std::filesystem::is_regular_file(wavPath, fileError) ||
        fileError ||
        std::filesystem::file_size(wavPath, fileError) <= 44 ||
        fileError) {
        throw std::runtime_error(
            "The Rust service finished, but no valid WAV file was produced."
        );
    }

    std::cout
        << "[Voice] Speaking with NVIDIA Qwen3-TTS custom voice.\n";

        if (!PlaySoundW(
            wavPath.c_str(),
            nullptr,
            SND_FILENAME | SND_SYNC | SND_NODEFAULT
        )) {
        throw std::runtime_error(
            "Windows could not play the generated voice WAV."
        );
    }

    // Conversation audio is temporary and private.
    if (!DeleteFileW(wavPath.c_str())) {
        std::cerr
            << "[Voice] Warning: Could not delete temporary WAV. Windows error: "
            << GetLastError() << '\n';
    }

    // Successful diagnostic logs are no longer needed.
    DeleteFileW(logPath.c_str());

    return true;
}

} // namespace

std::string captureAndTranscribeOnce() {
    const std::string modelPath = configuredModelPath();

    if (!std::filesystem::is_regular_file(modelPath)) {
        throw std::runtime_error(
            "Whisper model not found at '" + modelPath +
            "'. Download the base.en model into the project's models "
            "directory, or set NUMERICK_WHISPER_MODEL to its path."
        );
    }

    std::cout << "NUMERICK > Loading local Whisper model...\n";

    std::call_once(ggmlBackendLoadOnce, [] {
    ggml_backend_load_all();
});

    auto contextParams = whisper_context_default_params();
    contextParams.use_gpu = false;

    std::unique_ptr<whisper_context, WhisperContextDeleter> context(
        whisper_init_from_file_with_params(
            modelPath.c_str(),
            contextParams
        )
    );

    if (!context) {
        throw std::runtime_error(
            "Whisper could not load model: " + modelPath
        );
    }

    AudioSubsystem audioSubsystem;
    AudioCapture capture;

    SDL_AudioSpec desired{};
    SDL_AudioSpec obtained{};

    desired.freq = kSampleRate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = captureCallback;
    desired.userdata = &capture;

    AudioDevice device;
    device.id = SDL_OpenAudioDevice(
        nullptr,
        SDL_TRUE,
        &desired,
        &obtained,
        0
    );

    if (device.id == 0) {
        throw std::runtime_error(
            std::string("Could not open the microphone: ") +
            SDL_GetError()
        );
    }

    if (obtained.freq != kSampleRate ||
        obtained.format != AUDIO_F32SYS ||
        obtained.channels != 1) {
        throw std::runtime_error(
            "The microphone did not provide the required 16 kHz, "
            "mono, 32-bit float format."
        );
    }

    std::cout
        << "\nNUMERICK > Listening. Speak now, then press ENTER to stop.\n"
        << "Maximum recording length: "
        << kMaxRecordingSeconds << " seconds.\n";

    SDL_PauseAudioDevice(device.id, 0);

    std::string stopInput;
    std::getline(std::cin, stopInput);

    SDL_PauseAudioDevice(device.id, 1);
    SDL_CloseAudioDevice(device.id);
    device.id = 0;

    if (capture.invalidFormat) {
        throw std::runtime_error(
            "The microphone returned an unexpected audio format."
        );
    }

    if (capture.written < static_cast<std::size_t>(kSampleRate / 4)) {
        throw std::runtime_error(
            "Too little audio was captured. Try speaking for longer."
        );
    }

    if (capture.truncated) {
        std::cout
            << "NUMERICK > Recording reached the "
            << kMaxRecordingSeconds << "-second limit.\n";
    }

    auto params =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.n_threads = static_cast<int>(
        std::min(
            8U,
            std::max(2U, std::thread::hardware_concurrency())
        )
    );

    const std::string language = configuredLanguage();
    params.language = language.c_str();
    params.detect_language = (language == "auto");
    params.no_context = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    std::cout << "NUMERICK > Transcribing locally...\n";

    const int result = whisper_full(
        context.get(),
        params,
        capture.samples.data(),
        static_cast<int>(capture.written)
    );

    if (result != 0) {
        throw std::runtime_error("Whisper failed to transcribe the audio.");
    }

    std::string transcript;
    const int segments = whisper_full_n_segments(context.get());

    for (int i = 0; i < segments; ++i) {
        const char* segment =
            whisper_full_get_segment_text(context.get(), i);

        if (segment != nullptr) {
            transcript += segment;
        }
    }

    transcript = trim(std::move(transcript));

    if (transcript.empty()) {
        throw std::runtime_error(
            "Whisper did not recognize any speech. Try again."
        );
    }

    return transcript;
}

bool speakWithPersistentNvidiaQwen(const std::string& text) {
    const std::wstring serviceValue =
        readWideEnvironmentVariable(L"NUMERICK_VOICE_SERVICE_EXE");

    const std::wstring outputValue =
        readWideEnvironmentVariable(L"NUMERICK_TTS_OUTPUT_DIR");

    if (serviceValue.empty() || outputValue.empty()) {
        return false;
    }

    static std::mutex clientMutex;
    static std::unique_ptr<PersistentVoiceClient> client;

    std::lock_guard<std::mutex> lock(clientMutex);

    try {
        if (!client) {
            std::cout
                << "[Voice] Starting persistent NVIDIA voice engine...\n";

            client = std::make_unique<PersistentVoiceClient>();

            std::cout
                << "[Voice] Persistent voice engine is ready.\n";
        }

        client->speak(text);
        return true;
    }
    catch (...) {
        if (client) {
            client->shutdown();
            client.reset();
        }

        throw;
    }
}

void speakText(const std::string& text) {
    if (text.empty()) {
        return;
    }

    // Primary: persistent Rust + NVIDIA Qwen3-TTS.
    try {
        if (speakWithPersistentNvidiaQwen(text)) {
            return;
        }
    }
    catch (const std::exception& error) {
        std::cerr
            << "[Voice] Persistent NVIDIA engine failed: "
            << error.what() << '\n';
    }

    // Secondary: existing one-shot NVIDIA implementation.
    try {
        if (speakWithNvidiaQwen(text)) {
            return;
        }
    }
    catch (const std::exception& error) {
        std::cerr
            << "[Voice] One-shot NVIDIA engine failed: "
            << error.what() << '\n';
    }

    std::cout
        << "[Voice] Using Windows SAPI fallback.\n";

    // Final fallback: existing Windows speech implementation.
    const std::wstring wideText = utf8ToWide(text);
    ComApartment apartment;

    ISpVoice* rawVoice = nullptr;

    const HRESULT createResult = CoCreateInstance(
        CLSID_SpVoice,
        nullptr,
        CLSCTX_ALL,
        IID_ISpVoice,
        reinterpret_cast<void**>(&rawVoice)
    );

    if (FAILED(createResult) || rawVoice == nullptr) {
        throw std::runtime_error(
            "Windows SAPI could not create a speech voice. HRESULT=" +
            std::to_string(static_cast<unsigned long>(createResult))
        );
    }

    std::unique_ptr<ISpVoice, SpVoiceDeleter> voice(rawVoice);

    const HRESULT result =
        voice->Speak(wideText.c_str(), SPF_DEFAULT, nullptr);

    if (FAILED(result)) {
        throw std::runtime_error(
            "Windows SAPI failed to speak the response. HRESULT=" +
            std::to_string(static_cast<unsigned long>(result))
        );
    }
}

} // namespace numerick