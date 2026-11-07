#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <mmsystem.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>

#include <nvigi.h>
#include <nvigi_ai.h>
#include <nvigi_cpu.h>
#include <nvigi_tts_qwen3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxFrameBytes = 32U * 1024U;
constexpr std::size_t kMaxTextUtf16Units = 8'000;
constexpr std::size_t kMaxSpeakerJsonBytes = 64U * 1024U * 1024U;
constexpr int kExpectedSampleRate = 24'000;
constexpr int kExpectedChannels = 1;
constexpr int kExpectedBitsPerSample = 16;
constexpr int kMaxWordsPerChunk = 40;
constexpr std::size_t kMaxAudioSamples =
    static_cast<std::size_t>(kExpectedSampleRate) * 60U * 15U; // 15 minutes

std::wstring getEnvironmentVariable(const wchar_t* name) {
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) {
        return {};
    }

    std::wstring value(static_cast<std::size_t>(needed), L'\0');
    const DWORD written = GetEnvironmentVariableW(
        name,
        value.data(),
        needed
    );

    if (written == 0 || written >= needed) {
        return {};
    }

    value.resize(static_cast<std::size_t>(written));
    return value;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int needed = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (needed <= 0) {
        throw std::runtime_error("Could not convert a Windows path to UTF-8.");
    }

    std::string result(static_cast<std::size_t>(needed), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            needed,
            nullptr,
            nullptr
        ) != needed) {
        throw std::runtime_error("Could not convert a Windows path to UTF-8.");
    }

    return result;
}

std::size_t utf16Length(const std::string& utf8) {
    if (utf8.empty()) {
        return 0;
    }

    const int needed = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0
    );

    if (needed <= 0) {
        throw std::runtime_error("Speech request is not valid UTF-8.");
    }

    return static_cast<std::size_t>(needed);
}

std::string trimAndNormalizeWhitespace(std::string value) {
    std::string result;
    result.reserve(value.size());
    bool previousWasSpace = true;

    for (const unsigned char ch : value) {
        if (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ') {
            if (!previousWasSpace) {
                result.push_back(' ');
                previousWasSpace = true;
            }
        }
        else {
            result.push_back(static_cast<char>(ch));
            previousWasSpace = false;
        }
    }

    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

// A deliberately small JSON reader used only for NVIDIA's local Qwen speaker
// profile. It supports objects, arrays, strings, numbers, booleans and null.
// It avoids adding a third-party JSON dependency to this standalone worker.
struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    const JsonValue* find(const std::string& key) const {
        const auto* object = std::get_if<Object>(&value);
        if (!object) {
            return nullptr;
        }
        const auto it = object->find(key);
        return it == object->end() ? nullptr : &it->second;
    }

    const Array* array() const { return std::get_if<Array>(&value); }
    const std::string* string() const { return std::get_if<std::string>(&value); }
    const double* number() const { return std::get_if<double>(&value); }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    JsonValue parse() {
        skipWhitespace();
        JsonValue result = parseValue();
        skipWhitespace();
        if (position_ != input_.size()) {
            fail("Unexpected data after the JSON value");
        }
        return result;
    }

private:
    const std::string& input_;
    std::size_t position_ = 0;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(
            std::string("Invalid speaker JSON at byte ") +
            std::to_string(position_) + ": " + message
        );
    }

    void skipWhitespace() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    char peek() const {
        return position_ < input_.size() ? input_[position_] : '\0';
    }

    bool consume(char expected) {
        if (peek() == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    static void appendUtf8(std::string& target, std::uint32_t codePoint) {
        if (codePoint <= 0x7F) {
            target.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF) {
            target.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF) {
            target.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else {
            target.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            target.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    std::uint32_t parseHex4() {
        if (position_ + 4 > input_.size()) {
            fail("Incomplete Unicode escape");
        }
        std::uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = input_[position_++];
            code <<= 4;
            if (ch >= '0' && ch <= '9') code |= static_cast<std::uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') code |= static_cast<std::uint32_t>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') code |= static_cast<std::uint32_t>(ch - 'A' + 10);
            else fail("Invalid Unicode escape");
        }
        return code;
    }

    std::string parseString() {
        if (!consume('"')) {
            fail("Expected a string");
        }

        std::string result;
        while (position_ < input_.size()) {
            const char ch = input_[position_++];
            if (ch == '"') {
                return result;
            }
            if (static_cast<unsigned char>(ch) < 0x20) {
                fail("Control character in string");
            }
            if (ch != '\\') {
                result.push_back(ch);
                continue;
            }

            if (position_ >= input_.size()) {
                fail("Incomplete string escape");
            }

            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                std::uint32_t codePoint = parseHex4();
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                    if (position_ + 2 > input_.size() ||
                        input_[position_] != '\\' || input_[position_ + 1] != 'u') {
                        fail("Missing low surrogate");
                    }
                    position_ += 2;
                    const std::uint32_t low = parseHex4();
                    if (low < 0xDC00 || low > 0xDFFF) {
                        fail("Invalid low surrogate");
                    }
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(result, codePoint);
                break;
            }
            default: fail("Unknown string escape");
            }
        }

        fail("Unterminated string");
    }

    JsonValue parseNumber() {
        const std::size_t start = position_;
        if (consume('-')) {}
        if (consume('0')) {}
        else {
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("Invalid number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++position_;
        }
        if (consume('.')) {
            if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("Invalid fraction");
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++position_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '+' || peek() == '-') ++position_;
            if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("Invalid exponent");
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++position_;
        }
        const std::string token = input_.substr(start, position_ - start);
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
            fail("Number is out of range");
        }
        return JsonValue{parsed};
    }

    JsonValue parseArray() {
        consume('[');
        JsonValue::Array values;
        skipWhitespace();
        if (consume(']')) return JsonValue{std::move(values)};

        for (;;) {
            skipWhitespace();
            values.push_back(parseValue());
            skipWhitespace();
            if (consume(']')) break;
            if (!consume(',')) fail("Expected ',' in array");
        }
        return JsonValue{std::move(values)};
    }

    JsonValue parseObject() {
        consume('{');
        JsonValue::Object values;
        skipWhitespace();
        if (consume('}')) return JsonValue{std::move(values)};

        for (;;) {
            skipWhitespace();
            if (peek() != '"') fail("Expected object key");
            std::string key = parseString();
            skipWhitespace();
            if (!consume(':')) fail("Expected ':' after object key");
            skipWhitespace();
            values.insert_or_assign(std::move(key), parseValue());
            skipWhitespace();
            if (consume('}')) break;
            if (!consume(',')) fail("Expected ',' in object");
        }
        return JsonValue{std::move(values)};
    }

    JsonValue parseValue() {
        skipWhitespace();
        switch (peek()) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return JsonValue{parseString()};
        case 't':
            if (input_.compare(position_, 4, "true") == 0) {
                position_ += 4;
                return JsonValue{true};
            }
            break;
        case 'f':
            if (input_.compare(position_, 5, "false") == 0) {
                position_ += 5;
                return JsonValue{false};
            }
            break;
        case 'n':
            if (input_.compare(position_, 4, "null") == 0) {
                position_ += 4;
                return JsonValue{nullptr};
            }
            break;
        default:
            if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
                return parseNumber();
            }
        }
        fail("Invalid JSON value");
    }
};

const JsonValue* findFirstKey(
    const JsonValue& root,
    std::initializer_list<const char*> names
) {
    for (const char* name : names) {
        if (const JsonValue* value = root.find(name)) {
            return value;
        }
    }
    return nullptr;
}

std::vector<std::uint8_t> decodeBase64(const std::string& encoded) {
    static const auto table = [] {
        constexpr signed char kInvalid = -1;
        std::array<signed char, 256> values{};
        values.fill(kInvalid);
        const std::string alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::size_t i = 0; i < alphabet.size(); ++i) {
            values[static_cast<unsigned char>(alphabet[i])] =
                static_cast<signed char>(i);
        }
        values[static_cast<unsigned char>('-')] = 62;
        values[static_cast<unsigned char>('_')] = 63;
        return values;
    }();

    std::vector<std::uint8_t> decoded;
    decoded.reserve((encoded.size() * 3U) / 4U);
    std::uint32_t accumulator = 0;
    int bits = 0;
    bool sawPadding = false;

    for (const unsigned char ch : encoded) {
        if (std::isspace(ch)) {
            continue;
        }
        if (ch == '=') {
            sawPadding = true;
            continue;
        }
        if (sawPadding || table[ch] < 0) {
            throw std::runtime_error("Speaker JSON contains invalid Base64 data.");
        }

        accumulator = (accumulator << 6U) |
            static_cast<std::uint32_t>(table[ch]);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded.push_back(static_cast<std::uint8_t>(
                (accumulator >> static_cast<unsigned>(bits)) & 0xFFU
            ));
        }
    }

    if (bits == 6 || (bits > 0 && (accumulator & ((1U << bits) - 1U)) != 0U)) {
        throw std::runtime_error("Speaker JSON contains malformed Base64 padding.");
    }
    return decoded;
}

const JsonValue* encodedData(const JsonValue& value) {
    for (const char* key : {"bin_data", "data", "data_b64", "base64", "values", "value"}) {
        if (const JsonValue* data = value.find(key)) {
            return data;
        }
    }
    return &value;
}

std::uint32_t readLittleEndianU32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void flattenFloatValues(const JsonValue& value, std::vector<double>& output) {
    if (const auto* number = value.number()) {
        output.push_back(*number);
        return;
    }
    if (const auto* array = value.array()) {
        for (const auto& child : *array) {
            flattenFloatValues(child, output);
        }
        return;
    }

    const JsonValue* data = encodedData(value);
    if (data != nullptr && data != &value) {
        flattenFloatValues(*data, output);
        return;
    }
    if (const auto* base64 = value.string()) {
        const auto bytes = decodeBase64(*base64);
        if (bytes.empty() || bytes.size() % sizeof(float) != 0) {
            throw std::runtime_error(
                "The Base64 speaker embedding is not a non-empty float32 payload."
            );
        }
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4U) {
            const std::uint32_t bits = readLittleEndianU32(bytes.data() + offset);
            float sample = 0.0F;
            static_assert(sizeof(sample) == sizeof(bits), "float32 is required");
            std::memcpy(&sample, &bits, sizeof(sample));
            output.push_back(static_cast<double>(sample));
        }
    }
}

void flattenInt32Values(const JsonValue& value, std::vector<double>& output) {
    if (const auto* number = value.number()) {
        output.push_back(*number);
        return;
    }
    if (const auto* array = value.array()) {
        for (const auto& child : *array) {
            flattenInt32Values(child, output);
        }
        return;
    }

    const JsonValue* data = encodedData(value);
    if (data != nullptr && data != &value) {
        flattenInt32Values(*data, output);
        return;
    }
    if (const auto* base64 = value.string()) {
        const auto bytes = decodeBase64(*base64);
        if (bytes.empty() || bytes.size() % sizeof(std::int32_t) != 0) {
            throw std::runtime_error(
                "The Base64 reference-code payload is not a non-empty int32 array."
            );
        }
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4U) {
            const std::uint32_t bits = readLittleEndianU32(bytes.data() + offset);
            std::int32_t sample = 0;
            std::memcpy(&sample, &bits, sizeof(sample));
            output.push_back(static_cast<double>(sample));
        }
    }
}

std::size_t inferSecondDimension(const JsonValue& value) {
    if (const auto* objectShape = value.find("shape")) {
        if (const auto* dimensions = objectShape->array();
            dimensions != nullptr && !dimensions->empty()) {
            const auto* last = dimensions->back().number();
            return last != nullptr && *last > 0.0
                ? static_cast<std::size_t>(*last) : 0U;
        }
    }

    const auto* outer = value.array();
    if (!outer || outer->empty()) return 0;
    const auto* firstRow = outer->front().array();
    return firstRow ? firstRow->size() : 0;
}

struct SpeakerReference {
    std::vector<float> embedding;
    std::vector<std::int32_t> codes;
    std::string transcript;
    int frameCount = 0;
};

SpeakerReference loadSpeakerReference(const fs::path& path) {
    std::error_code error;
    const auto fileSize = fs::file_size(path, error);
    if (error || fileSize == 0 || fileSize > kMaxSpeakerJsonBytes) {
        throw std::runtime_error("Speaker profile is missing, empty, or larger than 64 MB.");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open the Qwen speaker profile.");
    }

    std::string contents(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }

    const JsonValue root = JsonParser(contents).parse();
    SpeakerReference reference;

    const JsonValue* embedding = findFirstKey(
        root, {"spkEmb", "spk_emb", "speakerEmbedding", "speaker_embedding"}
    );
    if (embedding == nullptr) {
        throw std::runtime_error("The Qwen speaker JSON does not contain spkEmb.");
    }

    std::vector<double> embeddingValues;
    flattenFloatValues(*embedding, embeddingValues);
    if (embeddingValues.empty()) {
        throw std::runtime_error(
            "The Qwen speaker embedding has no numeric values. Expected a float32 "
            "Base64 payload in spk_emb.bin_data or a numeric array."
        );
    }
    reference.embedding.reserve(embeddingValues.size());
    for (double value : embeddingValues) {
        if (!std::isfinite(value) ||
            value < -static_cast<double>(std::numeric_limits<float>::max()) ||
            value > static_cast<double>(std::numeric_limits<float>::max())) {
            throw std::runtime_error("The speaker embedding contains an invalid value.");
        }
        reference.embedding.push_back(static_cast<float>(value));
    }

    if (const JsonValue* transcript = findFirstKey(root, {"refText", "ref_text"})) {
        if (const auto* text = transcript->string()) {
            reference.transcript = *text;
        }
    }

    if (const JsonValue* codes = findFirstKey(root, {"refCodes", "ref_codes"})) {
        std::vector<double> codeValues;
        flattenInt32Values(*codes, codeValues);
        for (double value : codeValues) {
            if (!std::isfinite(value) ||
                value < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                value > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
                std::floor(value) != value) {
                throw std::runtime_error("The speaker reference codes contain an invalid value.");
            }
            reference.codes.push_back(static_cast<std::int32_t>(value));
        }

        if (const JsonValue* frameCount = findFirstKey(
                root, {"refT", "ref_t", "refCodesT", "ref_codes_t"})) {
            if (const double* count = frameCount->number();
                count != nullptr && *count > 0.0 &&
                *count <= static_cast<double>(std::numeric_limits<int>::max())) {
                reference.frameCount = static_cast<int>(*count);
            }
        }
        if (reference.frameCount <= 0) {
            reference.frameCount = static_cast<int>(inferSecondDimension(*codes));
        }
        if (reference.codes.empty() || reference.frameCount <= 0 || reference.transcript.empty()) {
            // Keep the compatible x-vector-only path if ICL metadata is absent/incomplete.
            reference.codes.clear();
            reference.frameCount = 0;
        }
        else if (reference.codes.size() % static_cast<std::size_t>(reference.frameCount) != 0) {
            throw std::runtime_error("The speaker reference-code dimensions are inconsistent.");
        }
    }

    return reference;
}

struct QwenModelSelection {
    fs::path modelDirectory;
    fs::path talkerPath;
    fs::path codecPath;
    std::string guid;
};

QwenModelSelection resolveQwenModelSelection(const fs::path& modelsDirectory) {
    const fs::path pluginRoot = modelsDirectory / L"nvigi.plugin.tts.qwen3";
    std::error_code error;
    if (!fs::is_directory(pluginRoot, error) || error) {
        throw std::runtime_error(
            "Qwen plugin model directory is missing: " + pluginRoot.string()
        );
    }

    std::vector<QwenModelSelection> q4Candidates;
    for (const auto& entry : fs::directory_iterator(pluginRoot, error)) {
        if (error) {
            throw std::runtime_error(
                "Could not enumerate Qwen model directory: " + pluginRoot.string()
            );
        }
        if (!entry.is_directory(error) || error) {
            error.clear();
            continue;
        }

        const fs::path directory = entry.path();
        const fs::path talker = directory / L"qwen-talker-0.6b-base-Q4_K_M.gguf";
        const fs::path codec = directory / L"qwen-tokenizer-12hz-F32.gguf";
        if (!fs::is_regular_file(talker, error) || error) {
            error.clear();
            continue;
        }
        if (!fs::is_regular_file(codec, error) || error) {
            error.clear();
            continue;
        }

        QwenModelSelection candidate;
        candidate.modelDirectory = directory;
        candidate.talkerPath = talker;
        candidate.codecPath = codec;
        candidate.guid = directory.filename().string();
        q4Candidates.push_back(std::move(candidate));
    }

    if (q4Candidates.empty()) {
        throw std::runtime_error(
            "No complete Qwen Q4_K_M model pair was found under " +
            pluginRoot.string() + ". Expected a GUID subfolder containing "
            "qwen-talker-0.6b-base-Q4_K_M.gguf and qwen-tokenizer-12hz-F32.gguf."
        );
    }
    if (q4Candidates.size() > 1) {
        std::string folders;
        for (const auto& candidate : q4Candidates) {
            if (!folders.empty()) folders += ", ";
            folders += candidate.guid;
        }
        throw std::runtime_error(
            "Multiple Qwen Q4_K_M model folders were found. Refusing to guess: " + folders
        );
    }

    return std::move(q4Candidates.front());
}

struct WorkerConfig {
    fs::path runtimeDirectory;
    fs::path frameworkDll;
    fs::path modelsDirectory;
    fs::path speakerProfile;
    fs::path outputDirectory;
    fs::path modelDirectory;
    fs::path talkerPath;
    fs::path codecPath;
    std::string modelGuid;
    std::string runtimeDirectoryUtf8;
    std::string modelsDirectoryUtf8;
    std::string outputDirectoryUtf8;
    std::string talkerPathUtf8;
    std::string codecPathUtf8;
    std::string backend;
};

WorkerConfig loadConfig() {
    const std::wstring ttsExecutable = getEnvironmentVariable(L"NUMERICK_TTS_EXE");
    const std::wstring models = getEnvironmentVariable(L"NUMERICK_TTS_MODELS");
    const std::wstring speaker = getEnvironmentVariable(L"NUMERICK_TTS_SPEAKER");
    const std::wstring output = getEnvironmentVariable(L"NUMERICK_TTS_OUTPUT_DIR");

    if (ttsExecutable.empty() || models.empty() || speaker.empty() || output.empty()) {
        throw std::runtime_error(
            "NUMERICK_TTS_EXE, NUMERICK_TTS_MODELS, NUMERICK_TTS_SPEAKER, "
            "and NUMERICK_TTS_OUTPUT_DIR must all be set."
        );
    }

    WorkerConfig config;
    config.runtimeDirectory = fs::path(ttsExecutable).parent_path();
    config.frameworkDll = config.runtimeDirectory / L"nvigi.core.framework.dll";
    config.modelsDirectory = fs::path(models);
    config.speakerProfile = fs::path(speaker);
    config.outputDirectory = fs::path(output);

    if (!fs::is_regular_file(config.frameworkDll)) {
        throw std::runtime_error("NVIGI framework DLL is missing: " + config.frameworkDll.string());
    }
    if (!fs::is_directory(config.modelsDirectory)) {
        throw std::runtime_error("Qwen model directory is missing: " + config.modelsDirectory.string());
    }
    if (!fs::is_regular_file(config.speakerProfile)) {
        throw std::runtime_error("Qwen speaker profile is missing: " + config.speakerProfile.string());
    }

    // Do not hard-code NVIDIA's published example GUID here. The installed
    // pack's model GUID is the actual directory name under nvigi.plugin.tts.qwen3.
    // This also avoids mismatches between pack revisions that ship the same
    // Q4_K_M file under different model GUIDs.
    const QwenModelSelection model = resolveQwenModelSelection(config.modelsDirectory);
    config.modelDirectory = model.modelDirectory;
    config.talkerPath = model.talkerPath;
    config.codecPath = model.codecPath;
    config.modelGuid = model.guid;

    fs::create_directories(config.outputDirectory);
    config.runtimeDirectoryUtf8 = wideToUtf8(config.runtimeDirectory.wstring());
    config.modelsDirectoryUtf8 = wideToUtf8(config.modelsDirectory.wstring());
    config.outputDirectoryUtf8 = wideToUtf8(config.outputDirectory.wstring());
    config.talkerPathUtf8 = wideToUtf8(config.talkerPath.wstring());
    config.codecPathUtf8 = wideToUtf8(config.codecPath.wstring());

    std::wstring backend = getEnvironmentVariable(L"NUMERICK_TTS_BACKEND");
    if (backend.empty()) backend = L"cuda";
    config.backend = wideToUtf8(backend);
    std::transform(
        config.backend.begin(), config.backend.end(), config.backend.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    if (config.backend != "cuda") {
        throw std::runtime_error("The native NVIGI worker currently uses the CUDA plugin only.");
    }

    return config;
}

struct AudioCapture {
    std::vector<std::int16_t> samples;
    std::string error;
    int sampleRate = kExpectedSampleRate;
};

nvigi::InferenceExecutionState audioCallback(
    const nvigi::InferenceExecutionContext* context,
    nvigi::InferenceExecutionState state,
    void* userData
) {
    auto* capture = static_cast<AudioCapture*>(userData);
    if (capture == nullptr || context == nullptr) {
        return state;
    }

    if (!capture->error.empty()) {
        return state;
    }

    if (state == nvigi::kInferenceExecutionStateDataPending ||
        state == nvigi::kInferenceExecutionStateDone) {
        if (context->outputs == nullptr) {
            return state;
        }

        const nvigi::InferenceDataAudio* audio = nullptr;
        if (!context->outputs->findAndValidateSlot(
                nvigi::kQwen3TTSDataSlotGeneratedAudio,
                &audio
            ) || audio == nullptr || audio->audio == nullptr) {
            return state;
        }

        if (audio->dataType != nvigi::ePCM ||
            audio->bitsPerSample != kExpectedBitsPerSample ||
            audio->channels != kExpectedChannels ||
            audio->samplingRate != kExpectedSampleRate) {
            capture->error = "NVIGI returned audio in an unexpected format; expected 24 kHz mono 16-bit PCM.";
            return state;
        }

        const nvigi::CpuData* cpuData = nvigi::castTo<nvigi::CpuData>(audio->audio);
        if (cpuData == nullptr || cpuData->buffer == nullptr || cpuData->sizeInBytes == 0) {
            return state;
        }

        if ((cpuData->sizeInBytes % sizeof(std::int16_t)) != 0) {
            capture->error = "NVIGI returned an audio chunk with an invalid byte count.";
            return state;
        }

        const auto* samples = static_cast<const std::int16_t*>(cpuData->buffer);
        const std::size_t count = cpuData->sizeInBytes / sizeof(std::int16_t);
        if (count > kMaxAudioSamples - capture->samples.size()) {
            capture->error = "Generated speech exceeded the 15-minute audio safety limit.";
            return state;
        }

        // NVIGI owns this callback buffer and may invalidate it as soon as
        // this callback returns. Copy every chunk before returning, and never
        // allow a C++ allocation exception to cross the NVIGI callback ABI.
        try {
            capture->samples.insert(capture->samples.end(), samples, samples + count);
        }
        catch (...) {
            capture->error = "Could not allocate memory for the generated speech audio.";
        }
    }

    return state;
}

void writeU16(std::ostream& stream, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF)
    };
    stream.write(bytes, sizeof(bytes));
}

void writeU32(std::ostream& stream, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 24) & 0xFF)
    };
    stream.write(bytes, sizeof(bytes));
}

void writeWav(const fs::path& path, const std::vector<std::int16_t>& samples) {
    if (samples.empty()) {
        throw std::runtime_error("Qwen completed without returning any audio samples.");
    }

    const std::uint64_t dataBytes64 =
        static_cast<std::uint64_t>(samples.size()) * sizeof(std::int16_t);
    if (dataBytes64 > std::numeric_limits<std::uint32_t>::max() - 36U) {
        throw std::runtime_error("Generated audio is too large for a standard PCM WAV file.");
    }
    const auto dataBytes = static_cast<std::uint32_t>(dataBytes64);
    const std::uint32_t byteRate =
        kExpectedSampleRate * kExpectedChannels * (kExpectedBitsPerSample / 8);
    const std::uint16_t blockAlign =
        static_cast<std::uint16_t>(kExpectedChannels * (kExpectedBitsPerSample / 8));

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not create the temporary WAV file.");
    }

    output.write("RIFF", 4);
    writeU32(output, dataBytes + 36U);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    writeU32(output, 16U);
    writeU16(output, 1U);
    writeU16(output, static_cast<std::uint16_t>(kExpectedChannels));
    writeU32(output, static_cast<std::uint32_t>(kExpectedSampleRate));
    writeU32(output, byteRate);
    writeU16(output, blockAlign);
    writeU16(output, static_cast<std::uint16_t>(kExpectedBitsPerSample));
    output.write("data", 4);
    writeU32(output, dataBytes);
    output.write(
        reinterpret_cast<const char*>(samples.data()),
        static_cast<std::streamsize>(dataBytes)
    );
    output.flush();

    if (!output) {
        throw std::runtime_error("Writing the temporary WAV file failed.");
    }
}

class NvigiQwenHost {
public:
    explicit NvigiQwenHost(WorkerConfig config)
        : config_(std::move(config)), speaker_(loadSpeakerReference(config_.speakerProfile)) {
        try {
            initialize();
        }
        catch (...) {
            cleanup();
            throw;
        }
    }

    ~NvigiQwenHost() {
        cleanup();
    }

    NvigiQwenHost(const NvigiQwenHost&) = delete;
    NvigiQwenHost& operator=(const NvigiQwenHost&) = delete;

    void speak(const std::string& untrustedText) {
        const std::vector<std::int16_t> samples = synthesizeSamples(untrustedText);
        const std::uint64_t requestId = ++requestCounter_;
        const fs::path wavPath = config_.outputDirectory /
            (L"numerick-native-" + std::to_wstring(GetCurrentProcessId()) +
             L"-" + std::to_wstring(requestId) + L".wav");

        writeWav(wavPath, samples);
        const BOOL played = PlaySoundW(
            wavPath.wstring().c_str(),
            nullptr,
            SND_FILENAME | SND_SYNC | SND_NODEFAULT
        );
        const DWORD playbackError = played ? ERROR_SUCCESS : GetLastError();
        std::error_code removeError;
        fs::remove(wavPath, removeError);

        if (!played) {
            throw std::runtime_error(
                "Windows could not play the synthesized WAV (error " +
                std::to_string(playbackError) + ")."
            );
        }
    }

    fs::path synthesizeToFile(
        const std::string& text,
        const std::wstring& outputName
    ) {
        if (outputName.empty() || outputName.size() > 64 ||
            !std::all_of(outputName.begin(), outputName.end(), [](wchar_t ch) {
                return (ch >= L'a' && ch <= L'z') ||
                       (ch >= L'A' && ch <= L'Z') ||
                       (ch >= L'0' && ch <= L'9') ||
                       ch == L'-' || ch == L'_';
            })) {
            throw std::runtime_error("Output name must contain 1-64 safe ASCII characters.");
        }

        const fs::path outputPath = config_.outputDirectory / (outputName + L".wav");
        if (fs::exists(outputPath)) {
            throw std::runtime_error("The requested output WAV already exists.");
        }

        const std::vector<std::int16_t> samples = synthesizeSamples(text);
        writeWav(outputPath, samples);
        return outputPath;
    }

private:
    WorkerConfig config_;
    SpeakerReference speaker_;
    HMODULE framework_ = nullptr;
    PFun_nvigiInit* initFunction_ = nullptr;
    PFun_nvigiShutdown* shutdownFunction_ = nullptr;
    PFun_nvigiLoadInterface* loadInterfaceFunction_ = nullptr;
    PFun_nvigiUnloadInterface* unloadInterfaceFunction_ = nullptr;
    nvigi::ITextToSpeech* tts_ = nullptr;
    nvigi::InferenceInstance* instance_ = nullptr;
    bool initialized_ = false;
    std::uint64_t requestCounter_ = 0;

    void initialize() {
        // The worker is an isolated child process, so this directory setting
        // cannot perturb NUMERICK's MSYS2/MinGW process.
        if (!SetDllDirectoryW(config_.runtimeDirectory.wstring().c_str())) {
            throw std::runtime_error("Could not set the local NVIDIA runtime directory.");
        }

        framework_ = LoadLibraryExW(
            config_.frameworkDll.wstring().c_str(),
            nullptr,
            LOAD_WITH_ALTERED_SEARCH_PATH
        );
        if (framework_ == nullptr) {
            throw std::runtime_error(
                "Could not load nvigi.core.framework.dll (Windows error " +
                std::to_string(GetLastError()) + ")."
            );
        }

        initFunction_ = reinterpret_cast<PFun_nvigiInit*>(
            GetProcAddress(framework_, "nvigiInit")
        );
        shutdownFunction_ = reinterpret_cast<PFun_nvigiShutdown*>(
            GetProcAddress(framework_, "nvigiShutdown")
        );
        loadInterfaceFunction_ = reinterpret_cast<PFun_nvigiLoadInterface*>(
            GetProcAddress(framework_, "nvigiLoadInterface")
        );
        unloadInterfaceFunction_ = reinterpret_cast<PFun_nvigiUnloadInterface*>(
            GetProcAddress(framework_, "nvigiUnloadInterface")
        );

        if (initFunction_ == nullptr || shutdownFunction_ == nullptr ||
            loadInterfaceFunction_ == nullptr || unloadInterfaceFunction_ == nullptr) {
            throw std::runtime_error("NVIGI framework is missing one or more required exported functions.");
        }

        const char* pluginPaths[] = {config_.runtimeDirectoryUtf8.c_str()};
        nvigi::Preferences preferences{};
        preferences.utf8PathsToPlugins = pluginPaths;
        preferences.numPathsToPlugins = 1;
        preferences.utf8PathToDependencies = config_.runtimeDirectoryUtf8.c_str();
        preferences.utf8PathToLogsAndData = config_.outputDirectoryUtf8.c_str();

        const auto initResult = initFunction_(preferences, nullptr, nvigi::kSDKVersion);
        if (initResult != nvigi::kResultOk) {
            throw std::runtime_error(
                "nvigiInit failed (result " +
                std::to_string(static_cast<long long>(initResult)) + ")."
            );
        }
        initialized_ = true;

        nvigi::InferenceInterface interfaceVersion{};
        void* rawInterface = nullptr;
        const auto loadResult = loadInterfaceFunction_(
            nvigi::plugin::tts::qwen3::cuda::kId,
            interfaceVersion.s_type,
            interfaceVersion.getVersion(),
            &rawInterface,
            config_.runtimeDirectoryUtf8.c_str()
        );
        if (loadResult != nvigi::kResultOk || rawInterface == nullptr) {
            throw std::runtime_error(
                "Could not load the NVIDIA Qwen3-TTS CUDA plugin (result " +
                std::to_string(static_cast<long long>(loadResult)) + ")."
            );
        }
        tts_ = static_cast<nvigi::ITextToSpeech*>(rawInterface);

        nvigi::CommonCreationParameters common{};
        common.utf8PathToModels = config_.modelsDirectoryUtf8.c_str();
        common.modelGUID = config_.modelGuid.c_str();
        common.vramBudgetMB = 2048;

        nvigi::Qwen3TTSCreationParameters creation{};
        creation.talkerPath = config_.talkerPathUtf8.c_str();
        creation.codecPath = config_.codecPathUtf8.c_str();
        const auto chainResult = creation.chain(common);
        if (chainResult != nvigi::kResultOk) {
            throw std::runtime_error(
                "Could not chain NVIGI Qwen creation parameters (result " +
                std::to_string(static_cast<long long>(chainResult)) + ")."
            );
        }

        const auto createResult = tts_->createInstance(creation, &instance_);
        if (createResult != nvigi::kResultOk || instance_ == nullptr) {
            std::ostringstream resultCode;
            resultCode << "0x" << std::hex
                       << static_cast<unsigned long long>(createResult);
            throw std::runtime_error(
                "Could not create the Qwen3-TTS inference instance (result " +
                std::to_string(static_cast<long long>(createResult)) + ", " +
                resultCode.str() + "). Selected model GUID " + config_.modelGuid +
                "; talker " + config_.talkerPath.string() +
                "; codec " + config_.codecPath.string() +
                ". The worker now passes these exact installed paths to NVIGI; "
                "check the NVIGI diagnostic log for any remaining backend or VRAM cause."
            );
        }

        std::cerr << "[native-voice] Selected Qwen model GUID: " << config_.modelGuid << '\n';
        std::cerr << "[native-voice] Talker: " << config_.talkerPath.string() << '\n';
        std::cerr << "[native-voice] Codec: " << config_.codecPath.string() << '\n';
        std::cerr << "[native-voice] NVIGI Qwen3-TTS initialized; model remains loaded between requests.\n";
        std::cerr << "[native-voice] Speaker embedding: " << speaker_.embedding.size()
                  << " floats; ICL codes: " << speaker_.codes.size() << " values.\n";
    }

    std::vector<std::int16_t> synthesizeSamples(const std::string& untrustedText) {
        const std::string text = trimAndNormalizeWhitespace(untrustedText);
        if (text.empty()) {
            throw std::runtime_error("Speech text is empty.");
        }
        if (utf16Length(text) > kMaxTextUtf16Units) {
            throw std::runtime_error("Speech text exceeds the 8,000 UTF-16 unit limit.");
        }
        if (instance_ == nullptr) {
            throw std::runtime_error("Qwen inference instance is not initialized.");
        }

        AudioCapture capture;
        nvigi::CpuData textData{};
        textData.buffer = const_cast<char*>(text.c_str());
        textData.sizeInBytes = text.size() + 1;

        nvigi::InferenceDataText prompt{textData};
        nvigi::InferenceDataSlot inputSlot{
            nvigi::kQwen3TTSDataSlotPrompt,
            prompt
        };
        nvigi::InferenceDataSlotArray inputs{1, &inputSlot};

        nvigi::Qwen3TTSRuntimeParameters runtime{};
        runtime.language = "English";
        runtime.temperature = 0.9f;
        runtime.topK = 50;
        runtime.seed = -1;
        runtime.maxNewTokens = 2048;
        runtime.maxWordsPerChunk = kMaxWordsPerChunk;
        runtime.refSpkEmb = speaker_.embedding.data();
        runtime.refSpkDim = static_cast<int>(speaker_.embedding.size());
        if (!speaker_.codes.empty()) {
            runtime.refCodes = speaker_.codes.data();
            runtime.refT = speaker_.frameCount;
            runtime.refText = speaker_.transcript.c_str();
        }

        nvigi::InferenceExecutionContext execution{};
        execution.instance = instance_;
        execution.inputs = &inputs;
        execution.callback = &audioCallback;
        execution.callbackUserData = &capture;
        execution.runtimeParameters = runtime;

        const auto result = instance_->evaluate(&execution);
        if (result != nvigi::kResultOk) {
            throw std::runtime_error(
                "NVIGI Qwen inference failed (result " +
                std::to_string(static_cast<long long>(result)) + ")."
            );
        }
        if (!capture.error.empty()) {
            throw std::runtime_error(capture.error);
        }
        if (capture.samples.empty()) {
            throw std::runtime_error("Qwen completed without returning any audio samples.");
        }

        return capture.samples;
    }

    void cleanup() noexcept {
        if (instance_ != nullptr && tts_ != nullptr) {
            tts_->destroyInstance(instance_);
            instance_ = nullptr;
        }
        if (tts_ != nullptr && unloadInterfaceFunction_ != nullptr) {
            unloadInterfaceFunction_(nvigi::plugin::tts::qwen3::cuda::kId, tts_);
            tts_ = nullptr;
        }
        if (initialized_ && shutdownFunction_ != nullptr) {
            shutdownFunction_();
            initialized_ = false;
        }
        if (framework_ != nullptr) {
            FreeLibrary(framework_);
            framework_ = nullptr;
        }
    }
};

// The inherited stdout pipe is a protocol-only channel. Keep a private
// duplicate before redirecting process stdout to stderr: NVIGI and its
// dependencies are free to emit diagnostic text to stdout, which must never
// be allowed to corrupt the framed IPC stream consumed by NUMERICK.
HANDLE g_protocolOutputHandle = INVALID_HANDLE_VALUE;

void initializeProtocolOutput() {
    const HANDLE inheritedOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (inheritedOutput == nullptr || inheritedOutput == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("NUMERICK did not provide a valid protocol output pipe.");
    }

    if (!DuplicateHandle(
            GetCurrentProcess(),
            inheritedOutput,
            GetCurrentProcess(),
            &g_protocolOutputHandle,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS
        )) {
        throw std::runtime_error(
            "Could not preserve the NUMERICK protocol output pipe (Windows error " +
            std::to_string(GetLastError()) + ")."
        );
    }

    const HANDLE diagnosticOutput = GetStdHandle(STD_ERROR_HANDLE);
    if (diagnosticOutput == nullptr || diagnosticOutput == INVALID_HANDLE_VALUE) {
        CloseHandle(g_protocolOutputHandle);
        g_protocolOutputHandle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("NUMERICK did not provide a valid diagnostic output handle.");
    }

    // Redirect Win32 stdout and the MSVC CRT stdout descriptor to stderr.
    // std::cout/printf and any NVIGI diagnostics therefore go to the log,
    // while writeFrame() below writes only to the duplicated pipe handle.
    if (!SetStdHandle(STD_OUTPUT_HANDLE, diagnosticOutput)) {
        const DWORD error = GetLastError();
        CloseHandle(g_protocolOutputHandle);
        g_protocolOutputHandle = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
            "Could not redirect worker stdout to diagnostics (Windows error " +
            std::to_string(error) + ")."
        );
    }

    const int stderrFd = _fileno(stderr);
    const int stdoutFd = _fileno(stdout);
    if (stderrFd < 0 || stdoutFd < 0 || _dup2(stderrFd, stdoutFd) != 0) {
        CloseHandle(g_protocolOutputHandle);
        g_protocolOutputHandle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("Could not redirect the MSVC stdout stream to stderr.");
    }
}

void writeProtocolBytes(const char* data, std::size_t size) {
    if (g_protocolOutputHandle == nullptr ||
        g_protocolOutputHandle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("The NUMERICK protocol output pipe is not initialized.");
    }

    std::size_t offset = 0;
    while (offset < size) {
        const DWORD requested = static_cast<DWORD>(
            std::min<std::size_t>(size - offset, 64U * 1024U)
        );
        DWORD written = 0;
        if (!WriteFile(
                g_protocolOutputHandle,
                data + offset,
                requested,
                &written,
                nullptr
            ) || written == 0) {
            throw std::runtime_error(
                "Could not write a framed response to NUMERICK (Windows error " +
                std::to_string(GetLastError()) + ")."
            );
        }
        offset += written;
    }
}

void writeFrame(const std::string& payload) {
    const std::string header = std::to_string(payload.size()) + "\n";
    writeProtocolBytes(header.data(), header.size());
    if (!payload.empty()) {
        writeProtocolBytes(payload.data(), payload.size());
    }
    constexpr char terminator = '\n';
    writeProtocolBytes(&terminator, 1);
}

bool readFrame(std::string& payload) {
    std::string header;
    if (!std::getline(std::cin, header)) {
        return false;
    }
    if (header.empty() || header.size() > 8 ||
        !std::all_of(header.begin(), header.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        throw std::runtime_error("Received an invalid IPC frame length header.");
    }

    std::size_t length = 0;
    try {
        length = static_cast<std::size_t>(std::stoull(header));
    }
    catch (...) {
        throw std::runtime_error("Received an invalid IPC frame length.");
    }
    if (length == 0 || length > kMaxFrameBytes) {
        throw std::runtime_error("Received an IPC frame outside the permitted size.");
    }

    payload.assign(length, '\0');
    std::cin.read(payload.data(), static_cast<std::streamsize>(length));
    if (static_cast<std::size_t>(std::cin.gcount()) != length) {
        return false;
    }

    char terminator = '\0';
    if (!std::cin.get(terminator) || terminator != '\n') {
        throw std::runtime_error("Received an IPC frame without its newline terminator.");
    }
    return true;
}

std::string makeErrorPayload(const std::string& message) {
    std::string safe = message;
    std::replace(safe.begin(), safe.end(), '\r', ' ');
    std::replace(safe.begin(), safe.end(), '\n', ' ');
    return "ERR\n" + safe;
}

int runPersistentWorker() {
    _setmode(_fileno(stdin), _O_BINARY);
    initializeProtocolOutput();

    std::unique_ptr<NvigiQwenHost> host;
    try {
        WorkerConfig config = loadConfig();
        host = std::make_unique<NvigiQwenHost>(std::move(config));
    }
    catch (const std::exception& error) {
        std::cerr << "[native-voice] Startup failed: " << error.what() << '\n';
        try { writeFrame(makeErrorPayload(error.what())); } catch (...) {}
        return 2;
    }

    try {
        writeFrame("READY");
        std::string command;
        while (readFrame(command)) {
            if (command == "SHUTDOWN") {
                writeFrame("OK");
                break;
            }

            constexpr char kSpeakPrefix[] = "SPEAK\n";
            if (command.compare(0, sizeof(kSpeakPrefix) - 1, kSpeakPrefix) != 0) {
                writeFrame(makeErrorPayload("Unknown worker command."));
                continue;
            }

            const std::string text = command.substr(sizeof(kSpeakPrefix) - 1);
            try {
                host->speak(text);
                writeFrame("OK");
            }
            catch (const std::exception& error) {
                std::cerr << "[native-voice] Synthesis failed: " << error.what() << '\n';
                writeFrame(makeErrorPayload(error.what()));
            }
        }
    }
    catch (const std::exception& error) {
        std::cerr << "[native-voice] IPC failure: " << error.what() << '\n';
        return 3;
    }

    return 0;
}

} // namespace

int runOneShot(int argc, wchar_t** argv) {
    std::wstring text;
    std::wstring outputName;
    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--text") {
            if (i + 1 >= argc || !text.empty()) {
                throw std::runtime_error("--text requires exactly one value.");
            }
            text = argv[++i];
        }
        else if (argument == L"--output-name") {
            if (i + 1 >= argc || !outputName.empty()) {
                throw std::runtime_error("--output-name requires exactly one value.");
            }
            outputName = argv[++i];
        }
        else if (argument == L"--help" || argument == L"-h") {
            std::cout << "NUMERICK native NVIGI worker\n"
                         "Persistent mode: numerick-nvigi-voice-worker.exe --persistent\n"
                         "One-shot mode:   numerick-nvigi-voice-worker.exe --text TEXT --output-name NAME\n";
            return 0;
        }
        else {
            throw std::runtime_error("Unknown argument supplied to native voice worker.");
        }
    }

    if (text.empty() || outputName.empty()) {
        throw std::runtime_error("One-shot mode requires --text and --output-name.");
    }

    const std::string textUtf8 = wideToUtf8(text);
    WorkerConfig config = loadConfig();
    NvigiQwenHost host(std::move(config));
    const fs::path outputPath = host.synthesizeToFile(textUtf8, outputName);
    std::cout << wideToUtf8(outputPath.wstring()) << '\n';
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc >= 2 && std::wstring(argv[1]) == L"--persistent") {
            return runPersistentWorker();
        }
        if (argc == 2 && std::wstring(argv[1]) == L"--check-speaker") {
            std::wstring profile = getEnvironmentVariable(L"NUMERICK_TTS_SPEAKER");
            if (profile.empty()) {
                const std::wstring userProfile = getEnvironmentVariable(L"USERPROFILE");
                if (userProfile.empty()) {
                    throw std::runtime_error("USERPROFILE is not set; cannot locate reference.json.");
                }
                profile = (fs::path(userProfile) / L"NumerickVoiceLab" / L"reference.json").wstring();
            }
            const SpeakerReference speaker = loadSpeakerReference(fs::path(profile));
            std::cout << "Speaker profile OK\n"
                      << "Embedding values: " << speaker.embedding.size() << "\n"
                      << "ICL code values: " << speaker.codes.size() << "\n"
                      << "ICL frame count: " << speaker.frameCount << "\n";
            return 0;
        }
        if (argc == 2 && std::wstring(argv[1]) == L"--check-model") {
            const WorkerConfig config = loadConfig();
            std::cout << "Qwen model pack OK\n"
                      << "Model GUID: " << config.modelGuid << "\n"
                      << "Talker: " << wideToUtf8(config.talkerPath.wstring()) << "\n"
                      << "Codec: " << wideToUtf8(config.codecPath.wstring()) << "\n";
            return 0;
        }
        return runOneShot(argc, argv);
    }
    catch (const std::exception& error) {
        std::cerr << "[native-voice] Fatal error: " << error.what() << '\n';
        return 1;
    }
}
