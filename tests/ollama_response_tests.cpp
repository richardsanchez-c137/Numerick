#include "numerick/ollama_response.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using Json = nlohmann::json;
using numerick::OllamaFailureKind;
using numerick::OllamaRequestError;

template <typename Action>
static void expectFailure(OllamaFailureKind expected, const Action& action) {
    bool matched = false;
    try {
        action();
    }
    catch (const OllamaRequestError& error) {
        matched = error.kind() == expected;
    }
    assert(matched);
}

static void testAcceptsNonEmptyAssistantText() {
    const Json response = numerick::parseOllamaChatResponse(
        200,
        R"({"message":{"role":"assistant","content":"Hello."}})",
        false
    );
    assert(response["message"]["content"] == "Hello.");
}

static void testAcceptsValidToolCallWhenToolsWereProvided() {
    const Json response = numerick::parseOllamaChatResponse(
        200,
        R"({"message":{"role":"assistant","content":"","tool_calls":[{"function":{"name":"get_local_time","arguments":{}}}]}})",
        true
    );
    assert(response["message"]["tool_calls"].size() == 1);
}

static void testRejectsMalformedJson() {
    expectFailure(OllamaFailureKind::MalformedJson, [] {
        numerick::parseOllamaChatResponse(200, R"({"message":)", false);
    });
}

static void testRejectsInvalidResponseShape() {
    expectFailure(OllamaFailureKind::InvalidResponse, [] {
        numerick::parseOllamaChatResponse(200, R"({"unexpected":true})", false);
    });
}

static void testRejectsEmptyAssistantText() {
    expectFailure(OllamaFailureKind::InvalidResponse, [] {
        numerick::parseOllamaChatResponse(
            200, R"({"message":{"role":"assistant","content":"  "}})", false
        );
    });
}

static void testRejectsToolCallsWhenToolsWereNotProvided() {
    expectFailure(OllamaFailureKind::InvalidResponse, [] {
        numerick::parseOllamaChatResponse(
            200,
            R"({"message":{"role":"assistant","content":"","tool_calls":[{"function":{"name":"get_local_time","arguments":{}}}]}})",
            false
        );
    });
}

static void testDistinguishesMissingModelFromHttpError() {
    expectFailure(OllamaFailureKind::ModelNotFound, [] {
        numerick::parseOllamaChatResponse(
            404, R"({"error":"model 'qwen3.5:4b' not found"})", false
        );
    });

    expectFailure(OllamaFailureKind::HttpStatus, [] {
        numerick::parseOllamaChatResponse(
            500, R"({"error":"internal server error"})", false
        );
    });
}

static void testRejectsOversizedResponseChunkWithoutMutatingBody() {
    std::string body = "1234";
    expectFailure(OllamaFailureKind::ResponseTooLarge, [&body] {
        numerick::appendOllamaResponseChunk(body, std::string_view("56"), 5);
    });
    assert(body == "1234");
}

static void testRejectsOversizedBodyBeforeParsing() {
    const std::string body(numerick::kMaxOllamaPayloadBytes + 1, 'x');
    expectFailure(OllamaFailureKind::ResponseTooLarge, [&body] {
        numerick::parseOllamaChatResponse(200, body, false);
    });
}

int main() {
    testAcceptsNonEmptyAssistantText();
    testAcceptsValidToolCallWhenToolsWereProvided();
    testRejectsMalformedJson();
    testRejectsInvalidResponseShape();
    testRejectsEmptyAssistantText();
    testRejectsToolCallsWhenToolsWereNotProvided();
    testDistinguishesMissingModelFromHttpError();
    testRejectsOversizedResponseChunkWithoutMutatingBody();
    testRejectsOversizedBodyBeforeParsing();
    std::cout << "All Ollama response tests passed.\n";
    return 0;
}
