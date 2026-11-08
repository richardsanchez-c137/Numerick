
#include "numerick/persistent_voice_client.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace numerick {
namespace {

constexpr std::size_t kMaxFrameBytes = 32000;

constexpr DWORD kStartupTimeoutMs = 90000;
constexpr DWORD kRequestTimeoutMs = 185000;
constexpr DWORD kShutdownAckTimeoutMs = 5000;
constexpr DWORD kShutdownWaitMs = 20000;
constexpr DWORD kTerminationWaitMs = 10000;

std::string windowsError(const char* operation) {
    return std::string(operation) +
        ". Windows error: " + std::to_string(GetLastError());
}

std::wstring readEnvironmentVariable(const wchar_t* name) {
    const DWORD required =
        GetEnvironmentVariableW(name, nullptr, 0);

    if (required == 0) {
        return {};
    }

    std::wstring result(required, L'\0');

    const DWORD written = GetEnvironmentVariableW(
        name,
        result.data(),
        required
    );

    if (written == 0 || written >= required) {
        return {};
    }

    result.resize(written);
    return result;
}

struct ScopedHandle {
    HANDLE value = nullptr;

    ScopedHandle() = default;

    explicit ScopedHandle(HANDLE handle)
        : value(handle) {}

    ~ScopedHandle() {
        close();
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    HANDLE release() noexcept {
        HANDLE result = value;
        value = nullptr;
        return result;
    }

    void close() noexcept {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
            value = nullptr;
        }
    }
};

} // namespace

class PersistentVoiceClient::Impl {
public:
    Impl() = default;

    ~Impl() {
        shutdown();
    }

    void start() {
        const std::wstring serviceValue =
            readEnvironmentVariable(
                L"NUMERICK_VOICE_SERVICE_EXE"
            );

        const std::wstring outputValue =
            readEnvironmentVariable(
                L"NUMERICK_TTS_OUTPUT_DIR"
            );

        if (serviceValue.empty() || outputValue.empty()) {
            throw std::runtime_error(
                "Persistent voice service paths are not configured."
            );
        }

        const std::filesystem::path servicePath(serviceValue);
        const std::filesystem::path outputDirectory(outputValue);

        if (!servicePath.is_absolute() ||
            !std::filesystem::is_regular_file(servicePath)) {
            throw std::runtime_error(
                "Persistent Rust voice executable was not found."
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
                "Could not access the voice output directory."
            );
        }

        logPath_ = outputDirectory /
            (L"numerick-persistent-" +
             std::to_wstring(GetCurrentProcessId()) +
             L"-" + std::to_wstring(GetTickCount64()) +
             L".log");

        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;

        ScopedHandle childStdin;
        ScopedHandle childStdout;
        ScopedHandle diagnosticLog;

        if (!CreatePipe(
                &childStdin.value,
                &inputPipe_,
                &security,
                0
            )) {
            throw std::runtime_error(
                windowsError("Could not create Rust input pipe")
            );
        }

        ScopedHandle parentInput(inputPipe_);
        inputPipe_ = nullptr;

        if (!SetHandleInformation(
                parentInput.value,
                HANDLE_FLAG_INHERIT,
                0
            )) {
            throw std::runtime_error(
                windowsError("Could not secure Rust input pipe")
            );
        }

        if (!CreatePipe(
                &outputPipe_,
                &childStdout.value,
                &security,
                0
            )) {
            throw std::runtime_error(
                windowsError("Could not create Rust output pipe")
            );
        }

        ScopedHandle parentOutput(outputPipe_);
        outputPipe_ = nullptr;

        if (!SetHandleInformation(
                parentOutput.value,
                HANDLE_FLAG_INHERIT,
                0
            )) {
            throw std::runtime_error(
                windowsError("Could not secure Rust output pipe")
            );
        }

        diagnosticLog.value = CreateFileW(
            logPath_.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            &security,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (diagnosticLog.value == INVALID_HANDLE_VALUE) {
            diagnosticLog.value = nullptr;

            throw std::runtime_error(
                windowsError("Could not create voice diagnostic log")
            );
        }

        job_ = CreateJobObjectW(nullptr, nullptr);

        if (job_ == nullptr) {
            throw std::runtime_error(
                windowsError("Could not create voice process job")
            );
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};

        limits.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(
                job_,
                JobObjectExtendedLimitInformation,
                &limits,
                sizeof(limits)
            )) {
            throw std::runtime_error(
                windowsError("Could not configure voice process job")
            );
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = childStdin.value;
        startup.hStdOutput = childStdout.value;
        startup.hStdError = diagnosticLog.value;

        PROCESS_INFORMATION processInfo{};

        std::wstring commandLine =
            L"\"" + servicePath.wstring() + L"\" --persistent";

        std::vector<wchar_t> mutableCommandLine(
            commandLine.begin(),
            commandLine.end()
        );

        mutableCommandLine.push_back(L'\0');

        const std::wstring workingDirectory =
            servicePath.parent_path().wstring();

        const BOOL started = CreateProcessW(
            servicePath.c_str(),
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED,
            nullptr,
            workingDirectory.c_str(),
            &startup,
            &processInfo
        );

        if (!started) {
            throw std::runtime_error(
                windowsError("Could not launch persistent Rust worker")
            );
        }

        process_ = processInfo.hProcess;
        ScopedHandle workerThread(processInfo.hThread);

        // Transfer parent-side pipe handles to this client.
        inputPipe_ = parentInput.release();
        outputPipe_ = parentOutput.release();

        // The child-side handles and diagnostic log are closed locally
        // when their ScopedHandle objects leave this function.

        if (!AssignProcessToJobObject(job_, process_)) {
            throw std::runtime_error(
                windowsError("Could not assign Rust worker to process job")
            );
        }

        if (ResumeThread(workerThread.value) ==
            static_cast<DWORD>(-1)) {
            throw std::runtime_error(
                windowsError("Could not resume Rust worker")
            );
        }

        const std::string response =
            receiveFrame(kStartupTimeoutMs);

        if (response != "READY") {
            throw std::runtime_error(
                "Rust worker did not become ready: " + response
            );
        }

        ready_ = true;

        OutputDebugStringW(
            L"[NUMERICK] Persistent voice worker is ready.\n"
        );
    }

    void speak(const std::string& text) {
        if (!ready_) {
            throw std::runtime_error(
                "Persistent voice worker is not ready."
            );
        }

        if (text.empty()) {
            return;
        }

        const std::string request = "SPEAK\n" + text;

        sendFrame(request);

        const std::string response =
            receiveFrame(kRequestTimeoutMs);

        if (response == "OK") {
            return;
        }

        if (response.rfind("ERR\n", 0) == 0) {
            throw std::runtime_error(response.substr(4));
        }

        throw std::runtime_error(
            "Unexpected response from persistent voice worker: " +
            response
        );
    }

    void shutdown() noexcept {
        if (process_ == nullptr) {
            closeHandles();
            return;
        }

        bool cleanShutdown = false;

        if (ready_ && inputPipe_ != nullptr &&
            outputPipe_ != nullptr &&
            WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
            try {
                sendFrame("SHUTDOWN");

                const std::string response =
                    receiveFrame(kShutdownAckTimeoutMs);

                if (response == "OK") {
                    cleanShutdown =
                        WaitForSingleObject(
                            process_,
                            kShutdownWaitMs
                        ) == WAIT_OBJECT_0;
                }
            }
            catch (...) {
                cleanShutdown = false;
            }
        }

        if (!cleanShutdown &&
            WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
            if (job_ != nullptr) {
                TerminateJobObject(job_, 1);
            }

            WaitForSingleObject(
                process_,
                kTerminationWaitMs
            );
        }

        ready_ = false;

        closeHandles();

        // Keep logs from failed sessions for troubleshooting.
        if (cleanShutdown && !logPath_.empty()) {
            DeleteFileW(logPath_.c_str());
        }
    }

private:
    HANDLE process_ = nullptr;
    HANDLE job_ = nullptr;
    HANDLE inputPipe_ = nullptr;
    HANDLE outputPipe_ = nullptr;

    std::filesystem::path logPath_;

    bool ready_ = false;

    void closeHandles() noexcept {
        if (inputPipe_ != nullptr) {
            CloseHandle(inputPipe_);
            inputPipe_ = nullptr;
        }

        if (outputPipe_ != nullptr) {
            CloseHandle(outputPipe_);
            outputPipe_ = nullptr;
        }

        if (process_ != nullptr) {
            CloseHandle(process_);
            process_ = nullptr;
        }

        if (job_ != nullptr) {
            CloseHandle(job_);
            job_ = nullptr;
        }
    }

    void writeAll(const char* data, std::size_t length) {
        std::size_t offset = 0;

        while (offset < length) {
            const DWORD requested = static_cast<DWORD>(
                std::min<std::size_t>(
                    length - offset,
                    MAXDWORD
                )
            );

            DWORD written = 0;

            if (!WriteFile(
                    inputPipe_,
                    data + offset,
                    requested,
                    &written,
                    nullptr
                )) {
                throw std::runtime_error(
                    windowsError("Writing to Rust worker failed")
                );
            }

            if (written == 0) {
                throw std::runtime_error(
                    "Rust worker accepted zero request bytes."
                );
            }

            offset += written;
        }
    }

    void sendFrame(const std::string& payload) {
        if (payload.size() > kMaxFrameBytes) {
            throw std::runtime_error(
                "Voice protocol request exceeds 32 KB."
            );
        }

        const std::string header =
            std::to_string(payload.size()) + "\n";

        writeAll(header.data(), header.size());
        writeAll(payload.data(), payload.size());

        const char terminator = '\n';
        writeAll(&terminator, 1);
    }

    void readExact(
        char* destination,
        std::size_t length,
        ULONGLONG deadline
    ) {
        std::size_t offset = 0;

        while (offset < length) {
            DWORD available = 0;

            if (!PeekNamedPipe(
                    outputPipe_,
                    nullptr,
                    0,
                    nullptr,
                    &available,
                    nullptr
                )) {
                throw std::runtime_error(
                    windowsError("Inspecting Rust response pipe failed")
                );
            }

            if (available == 0) {
                if (WaitForSingleObject(process_, 0) ==
                    WAIT_OBJECT_0) {
                    throw std::runtime_error(
                        "Rust voice worker exited unexpectedly."
                    );
                }

                if (GetTickCount64() >= deadline) {
                    throw std::runtime_error(
                        "Timed out waiting for Rust voice response."
                    );
                }

                Sleep(5);
                continue;
            }

            const DWORD requested = static_cast<DWORD>(
                std::min<std::size_t>(
                    length - offset,
                    available
                )
            );

            DWORD received = 0;

            if (!ReadFile(
                    outputPipe_,
                    destination + offset,
                    requested,
                    &received,
                    nullptr
                )) {
                throw std::runtime_error(
                    windowsError("Reading Rust response failed")
                );
            }

            if (received == 0) {
                throw std::runtime_error(
                    "Rust response pipe closed unexpectedly."
                );
            }

            offset += received;
        }
    }

    std::string receiveFrame(DWORD timeoutMs) {
        const ULONGLONG deadline =
            GetTickCount64() + timeoutMs;

        std::string header;

        while (true) {
            char character = '\0';

            readExact(&character, 1, deadline);

            if (character == '\n') {
                break;
            }

            if (character < '0' || character > '9' ||
                header.size() >= 10) {
                throw std::runtime_error(
                    "Invalid persistent voice protocol header."
                );
            }

            header.push_back(character);
        }

        if (header.empty()) {
            throw std::runtime_error(
                "Empty persistent voice protocol header."
            );
        }

        std::size_t length = 0;

        const auto parsed = std::from_chars(
            header.data(),
            header.data() + header.size(),
            length
        );

        if (parsed.ec != std::errc{} ||
            parsed.ptr != header.data() + header.size() ||
            length > kMaxFrameBytes) {
            throw std::runtime_error(
                "Invalid persistent voice response length."
            );
        }

        std::string payload(length, '\0');

        if (length != 0) {
            readExact(payload.data(), length, deadline);
        }

        char terminator = '\0';
        readExact(&terminator, 1, deadline);

        if (terminator != '\n') {
            throw std::runtime_error(
                "Invalid persistent voice response terminator."
            );
        }

        return payload;
    }
};

PersistentVoiceClient::PersistentVoiceClient()
    : impl_(std::make_unique<Impl>()) {
    impl_->start();
}

PersistentVoiceClient::~PersistentVoiceClient() = default;

void PersistentVoiceClient::speak(const std::string& text) {
    impl_->speak(text);
}

void PersistentVoiceClient::shutdown() noexcept {
    if (impl_) {
        impl_->shutdown();
    }
}

} // namespace numerick
