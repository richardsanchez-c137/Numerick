#include "numerick/ollama_config.hpp"

#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using numerick::OllamaConfig;
using numerick::OllamaHealthResult;
using numerick::evaluateOllamaTagsResponse;
using numerick::parseOllamaConfig;

static void testDefaults() {
    const OllamaConfig config = parseOllamaConfig({});
    assert(config.host == "127.0.0.1");
    assert(config.port == 11434);
    assert(config.model == "qwen3.5:4b");
    assert(config.responseTimeoutMs == 120000);
    assert(config.endpoint() == "127.0.0.1:11434");
}

static void testOverrides() {
    const OllamaConfig config = parseOllamaConfig({
        {"NUMERICK_OLLAMA_HOST", "ollama.local"},
        {"NUMERICK_OLLAMA_PORT", "12345"},
        {"NUMERICK_OLLAMA_MODEL", "qwen3.5:9b"},
        {"NUMERICK_OLLAMA_TIMEOUT_MS", "180000"}
    });
    assert(config.host == "ollama.local");
    assert(config.port == 12345);
    assert(config.model == "qwen3.5:9b");
    assert(config.responseTimeoutMs == 180000);
}

static void testInvalidPortRejected() {
    for (const std::string value : {"0", "65536", "-1", "abc", "11434x"}) {
        bool rejected = false;
        try {
            (void)parseOllamaConfig({{"NUMERICK_OLLAMA_PORT", value}});
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
}

static void testInvalidHostAndModelRejected() {
    for (const std::string host : {"", "http://localhost", "127.0.0.1/api", "host name"}) {
        bool rejected = false;
        try {
            (void)parseOllamaConfig({{"NUMERICK_OLLAMA_HOST", host}});
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    bool modelRejected = false;
    try {
        (void)parseOllamaConfig({{"NUMERICK_OLLAMA_MODEL", ""}});
    }
    catch (const std::invalid_argument&) {
        modelRejected = true;
    }
    assert(modelRejected);
}

static void testTimeoutBoundaries() {
    for (const std::string value : {"999", "300001", "abc"}) {
        bool rejected = false;
        try {
            (void)parseOllamaConfig({{"NUMERICK_OLLAMA_TIMEOUT_MS", value}});
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(parseOllamaConfig({
        {"NUMERICK_OLLAMA_TIMEOUT_MS", "1000"}
    }).responseTimeoutMs == 1000);
    assert(parseOllamaConfig({
        {"NUMERICK_OLLAMA_TIMEOUT_MS", "300000"}
    }).responseTimeoutMs == 300000);
}

static void testHealthReportsInstalledModel() {
    const OllamaHealthResult result = evaluateOllamaTagsResponse(
        200,
        R"({"models":[{"name":"llama3:latest"},{"name":"qwen3.5:4b"}]})",
        "qwen3.5:4b"
    );
    assert(result.serverResponded);
    assert(result.apiValid);
    assert(result.modelAvailable);
    assert(result.message.find("is installed") != std::string::npos);
}

static void testHealthReportsMissingModel() {
    const OllamaHealthResult result = evaluateOllamaTagsResponse(
        200, R"({"models":[{"name":"llama3:latest"}]})", "qwen3.5:4b"
    );
    assert(result.serverResponded);
    assert(result.apiValid);
    assert(!result.modelAvailable);
    assert(result.message.find("ollama pull qwen3.5:4b") != std::string::npos);
}

static void testHealthRejectsBadHttpAndJson() {
    const OllamaHealthResult httpFailure = evaluateOllamaTagsResponse(
        503, "not json", "qwen3.5:4b"
    );
    assert(httpFailure.serverResponded);
    assert(!httpFailure.apiValid);
    assert(!httpFailure.modelAvailable);

    const OllamaHealthResult jsonFailure = evaluateOllamaTagsResponse(
        200, "not json", "qwen3.5:4b"
    );
    assert(jsonFailure.serverResponded);
    assert(!jsonFailure.apiValid);

    const OllamaHealthResult shapeFailure = evaluateOllamaTagsResponse(
        200, R"({"unexpected":[]})", "qwen3.5:4b"
    );
    assert(shapeFailure.serverResponded);
    assert(!shapeFailure.apiValid);
}

int main() {
    testDefaults();
    testOverrides();
    testInvalidPortRejected();
    testInvalidHostAndModelRejected();
    testTimeoutBoundaries();
    testHealthReportsInstalledModel();
    testHealthReportsMissingModel();
    testHealthRejectsBadHttpAndJson();

    std::cout << "All Ollama config tests passed.\n";
    return 0;
}
