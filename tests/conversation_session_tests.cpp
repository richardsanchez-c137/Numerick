#include "numerick/conversation_session.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using numerick::ConversationSession;
using Json = nlohmann::json;

static const std::string kSystemPrompt =
    "You are NUMERICK, a helpful assistant.";

template <typename Action>
static void expectInvalidArgument(const Action& action) {
    bool threw = false;

    try {
        action();
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

static void testBuildsSystemAndCurrentUserMessages() {
    ConversationSession session;

    const Json messages =
        session.buildMessages("What is an API?", kSystemPrompt);

    assert(messages.size() == 2);
    assert(messages[0]["role"] == "system");
    assert(messages[0]["content"] == kSystemPrompt);
    assert(messages[1]["role"] == "user");
    assert(messages[1]["content"] == "What is an API?");
    assert(session.turnCount() == 0);
}

static void testFollowUpIncludesPreviousTurn() {
    ConversationSession session;

    session.recordTurn(
        "What is an API?",
        Json::array({
            {
                {"role", "assistant"},
                {"content", "An API lets software communicate."}
            }
        })
    );

    const Json messages = session.buildMessages(
        "Explain that with a simple analogy.",
        kSystemPrompt
    );

    assert(messages.size() == 4);
    assert(messages[0]["role"] == "system");
    assert(messages[1]["role"] == "user");
    assert(messages[1]["content"] == "What is an API?");
    assert(messages[2]["role"] == "assistant");
    assert(
        messages[2]["content"] ==
        "An API lets software communicate."
    );
    assert(messages[3]["role"] == "user");
    assert(
        messages[3]["content"] ==
        "Explain that with a simple analogy."
    );
    assert(session.turnCount() == 1);
}

static void testHistoryLimitDropsOldestWholeTurn() {
    ConversationSession session(2);

    session.recordTurn(
        "First question",
        Json::array({
            {{"role", "assistant"}, {"content", "First answer"}}
        })
    );
    session.recordTurn(
        "Second question",
        Json::array({
            {{"role", "assistant"}, {"content", "Second answer"}}
        })
    );
    session.recordTurn(
        "Third question",
        Json::array({
            {{"role", "assistant"}, {"content", "Third answer"}}
        })
    );

    const Json messages =
        session.buildMessages("Fourth question", kSystemPrompt);

    assert(session.maxTurns() == 2);
    assert(session.turnCount() == 2);

    // system + two stored user/assistant turns + current user = six.
    assert(messages.size() == 6);
    assert(messages[1]["content"] == "Second question");
    assert(messages[2]["content"] == "Second answer");
    assert(messages[3]["content"] == "Third question");
    assert(messages[4]["content"] == "Third answer");
    assert(messages[5]["content"] == "Fourth question");
}

static void testToolTranscriptRemainsIntactAcrossTurns() {
    ConversationSession session;

    const Json transcript = Json::array({
        {
            {"role", "assistant"},
            {"content", ""},
            {"tool_calls", Json::array({
                {
                    {"id", "call_time"},
                    {"type", "function"},
                    {"function", {
                        {"name", "get_local_time"},
                        {"arguments", Json::object()}
                    }}
                }
            })}
        },
        {
            {"role", "tool"},
            {"tool_name", "get_local_time"},
            {"content", "2026-10-25 15:00:00"}
        },
        {
            {"role", "assistant"},
            {"content", "The local time is 15:00."}
        }
    });

    session.recordTurn("What time is it?", transcript);

    const Json messages =
        session.buildMessages("How did you determine that?", kSystemPrompt);

    assert(messages.size() == 6);
    assert(messages[1]["role"] == "user");
    assert(messages[1]["content"] == "What time is it?");
    assert(messages[2]["role"] == "assistant");
    assert(messages[2].contains("tool_calls"));
    assert(messages[3]["role"] == "tool");
    assert(messages[3]["tool_name"] == "get_local_time");
    assert(messages[3]["content"] == "2026-10-25 15:00:00");
    assert(messages[4]["role"] == "assistant");
    assert(messages[4]["content"] == "The local time is 15:00.");
    assert(messages[5]["content"] == "How did you determine that?");
}

static void testRejectsIncompleteTranscriptAndBlankPrompts() {
    ConversationSession session;

    expectInvalidArgument([&]() {
        session.buildMessages("  \t", kSystemPrompt);
    });

    expectInvalidArgument([&]() {
        session.buildMessages("Question", " \n ");
    });

    expectInvalidArgument([&]() {
        session.recordTurn(
            "Question",
            Json::array({
                {
                    {"role", "tool"},
                    {"tool_name", "some_tool"},
                    {"content", "tool result"}
                }
            })
        );
    });

    expectInvalidArgument([&]() {
        session.recordTurn("Question", Json::array());
    });
}

static void testClearRemovesConversationHistory() {
    ConversationSession session;

    session.recordTurn(
        "Remember this",
        Json::array({
            {{"role", "assistant"}, {"content", "Stored for this session"}}
        })
    );

    assert(session.turnCount() == 1);

    session.clear();

    assert(session.turnCount() == 0);

    const Json messages = session.buildMessages("New topic", kSystemPrompt);
    assert(messages.size() == 2);
    assert(messages[1]["content"] == "New topic");
}

static void testRejectsBlankAssistantAnswerWithoutRecordingTurn() {
    ConversationSession session;
    expectInvalidArgument([&session] {
        session.recordTurn(
            "Question",
            Json::array({{{"role", "assistant"}, {"content", " \t\r\n"}}})
        );
    });
    assert(session.turnCount() == 0);
}

int main() {
    testBuildsSystemAndCurrentUserMessages();
    testFollowUpIncludesPreviousTurn();
    testHistoryLimitDropsOldestWholeTurn();
    testToolTranscriptRemainsIntactAcrossTurns();
    testRejectsIncompleteTranscriptAndBlankPrompts();
    testClearRemovesConversationHistory();
    testRejectsBlankAssistantAnswerWithoutRecordingTurn();

    std::cout << "All conversation session tests passed.\n";
    return 0;
}
