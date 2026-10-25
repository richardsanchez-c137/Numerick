#include "numerick/ai_client.hpp"
#include "numerick/builtin_tools.hpp"
#include "numerick/command_parser.hpp"
#include "numerick/conversation_session.hpp"
#include "numerick/tool_agent.hpp"
#include "numerick/tool_registry.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

constexpr const char* kAskSystemPrompt =
    "You are NUMERICK, a personal AI assistant. "
    "Answer clearly and directly. Be concise for ordinary questions. "
    "Never claim to have performed an action unless it was verified.";

constexpr const char* kAgentSystemPrompt =
    "You are NUMERICK, a personal AI assistant. "
    "Answer clearly and concisely. Use registered tools "
    "when they are appropriate for obtaining real "
    "information. When asked for the current local time, "
    "call get_local_time and use its returned result. "
    "Never guess the current time or claim a tool "
    "succeeded without its result. If a tool fails, "
    "explain that failure honestly. Use only the tools "
    "provided with this request.";

} // namespace

class Numerick {
public:
    Numerick()
        : aiClient_(),
          toolAgent_(aiClient_, registry_),
          conversation_(10) {
        fs::create_directories("data");
        logFile_.open("data/activity.log", std::ios::app);

        // Register only explicitly approved built-in tools.
        numerick::registerBuiltinTools(registry_);

        log("NUMERICK started");
    }

    ~Numerick() {
        log("NUMERICK stopped");

        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    void run() {
        printWelcome();

        std::string input;

        while (true) {
            std::cout << "\nYou > ";

            if (!std::getline(std::cin, input)) {
                break;
            }

            const numerick::ParsedCommand command =
                numerick::parseCommand(input);

            if (command.type == numerick::CommandType::Unknown &&
                command.argument.empty()) {
                continue;
            }

            // Do not record the user's entire AI prompt in the activity log.
            if (command.type == numerick::CommandType::Ask ||
                command.type == numerick::CommandType::Agent) {
                log(
                    command.type == numerick::CommandType::Agent
                        ? "Agent request submitted"
                        : "AI request submitted"
                );
            } else {
                log("Input received: " + input);
            }

            if (!handleCommand(command)) {
                break;
            }
        }
    }

private:
    numerick::ToolRegistry registry_;
    numerick::AIClient aiClient_;
    numerick::ToolAgent toolAgent_;
    numerick::ConversationSession conversation_;
    std::ofstream logFile_;

    static const char* stopReasonName(
        numerick::AgentStopReason reason
    ) {
        switch (reason) {
        case numerick::AgentStopReason::Completed:
            return "completed";

        case numerick::AgentStopReason::ToolRoundLimitReached:
            return "tool_round_limit";

        case numerick::AgentStopReason::ToolCallLimitReached:
            return "tool_call_limit";
        }

        return "unknown";
    }

    static std::string currentTime() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time =
            std::chrono::system_clock::to_time_t(now);

        std::tm local{};

#ifdef _WIN32
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif

        char buffer[32]{};

        std::strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%d %H:%M:%S",
            &local
        );

        return buffer;
    }

    void log(const std::string& message) {
        if (logFile_.is_open()) {
            logFile_ << "[" << currentTime() << "] "
                     << message << '\n';
            logFile_.flush();
        }
    }

    void printWelcome() const {
        std::cout
            << "\n"
            << "====================================\n"
            << "          N U M E R I C K\n"
            << "       Personal Assistant Core\n"
            << "====================================\n"
            << "Core version: 0.4.0\n"
            << "AI model: Qwen 3.5 4B via Ollama\n"
            << "Registered tools: "
            << registry_.definitions().size() << '\n'
            << "Conversation history: last "
            << conversation_.maxTurns()
            << " completed turns in memory\n"
            << "Type 'help' to see available commands.\n";
    }

    bool handleCommand(const numerick::ParsedCommand& command) {
        using numerick::CommandType;

        switch (command.type) {
        case CommandType::Help:
            showHelp();
            break;

        case CommandType::Greeting:
            std::cout
                << "NUMERICK > Hello. I'm online and ready.\n";
            break;

        case CommandType::Status:
            showStatus();
            break;

        case CommandType::Time:
            // Preserve the existing direct time command.
            std::cout
                << "NUMERICK > Local time: "
                << currentTime() << '\n';
            break;

        case CommandType::Clear:
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            // Clearing the terminal does not clear the conversation.
            printWelcome();
            break;

        case CommandType::Ask:
            if (command.argument.empty()) {
                std::cout
                    << "NUMERICK > Usage: ask <your question>\n";
                break;
            }

            std::cout << "NUMERICK > Thinking...\n";

            try {
                const Json messages = conversation_.buildMessages(
                    command.argument,
                    kAskSystemPrompt
                );

                const Json response = aiClient_.chat(messages);

                if (!response.is_object() ||
                    !response.contains("message") ||
                    !response["message"].is_object() ||
                    !response["message"].contains("content") ||
                    !response["message"]["content"].is_string()) {
                    throw std::runtime_error(
                        "Ollama response did not contain text content."
                    );
                }

                const Json assistantMessage = response["message"];
                const std::string answer =
                    assistantMessage["content"].get<std::string>();

                // Save the actual model message; do not include the UI-only
                // truncation warning in the conversational transcript.
                conversation_.recordTurn(
                    command.argument,
                    Json::array({assistantMessage})
                );

                std::cout << "\nNUMERICK > " << answer;

                if (response.value(
                        "done_reason", std::string{}) == "length") {
                    std::cout
                        << "\n\n[NUMERICK: The response may be incomplete "
                        << "because the output limit was reached.]";
                }

                std::cout << '\n';
                log("AI response received");
            }
            catch (const std::exception& error) {
                std::cout
                    << "\nNUMERICK > I couldn't get a response "
                    << "from the local AI model.\n"
                    << "Details: " << error.what() << '\n';

                log(
                    "AI request failed: " +
                    std::string(error.what())
                );
            }
            break;

        case CommandType::Agent:
            if (command.argument.empty()) {
                std::cout
                    << "NUMERICK > Usage: agent <your request>\n";
                break;
            }

            std::cout << "NUMERICK > Working on it...\n";

            try {
                const Json messages = conversation_.buildMessages(
                    command.argument,
                    kAgentSystemPrompt
                );

                const numerick::AgentRunReport report =
                    toolAgent_.runWithConversationDiagnostics(messages);

                // The report includes assistant tool-call messages, tool
                // results, and the final answer for this completed turn.
                conversation_.recordTurn(
                    command.argument,
                    report.turnMessages
                );

                std::cout << "\nNUMERICK > "
                          << report.answer << '\n';

                log(
                    "Agent diagnostics: model_requests=" +
                    std::to_string(report.modelRequests) +
                    ", tool_rounds=" +
                    std::to_string(report.toolRounds) +
                    ", tool_calls_requested=" +
                    std::to_string(report.toolCallsRequested) +
                    ", tool_calls_succeeded=" +
                    std::to_string(report.toolCallsSucceeded) +
                    ", tool_calls_failed=" +
                    std::to_string(report.toolCallsFailed) +
                    ", tool_calls_rejected=" +
                    std::to_string(report.toolCallsRejected) +
                    ", stop_reason=" +
                    stopReasonName(report.stopReason)
                );

                for (const auto& event : report.toolCalls) {
                    log(
                        "Agent tool outcome: " +
                        event.name + " / " + event.outcome
                    );
                }
            }
            catch (const std::exception& error) {
                std::cout
                    << "\nNUMERICK > The agent request failed.\n"
                    << "Details: " << error.what() << '\n';

                log(
                    "Agent request failed: " +
                    std::string(error.what())
                );
            }
            break;

        case CommandType::Exit:
            std::cout
                << "NUMERICK > Shutting down. Goodbye.\n";
            return false;

        case CommandType::Unknown:
            std::cout
                << "NUMERICK > I don't recognize that command. "
                << "Try 'help', 'ask <question>', or "
                << "'agent <request>'.\n";

            log("Unsupported command: " + command.argument);
            break;
        }

        return true;
    }

    void showHelp() const {
        std::cout
            << "\nAvailable commands:\n"
            << "  help              - Show available commands\n"
            << "  hello             - Test the assistant\n"
            << "  status            - Show current status\n"
            << "  time              - Display local time\n"
            << "  ask <question>    - Ask the AI with conversation context\n"
            << "  agent <request>   - Let AI request registered tools\n"
            << "  clear             - Clear the console (history remains)\n"
            << "  exit              - Shut down NUMERICK\n"
            << "\n";
    }

    void showStatus() const {
        std::cout
            << "\nNUMERICK STATUS\n"
            << "--------------------------------\n"
            << "Core:          Running\n"
            << "AI model:      Qwen 3.5 4B\n"
            << "AI runtime:    Local Ollama API\n"
            << "Registered tools: "
            << registry_.definitions().size() << '\n'
            << "Conversation history: "
            << conversation_.turnCount() << '/'
            << conversation_.maxTurns() << " turns\n"
            << "Voice input:   Not implemented\n"
            << "Vision:        Not implemented\n"
            << "Persistent memory: Not implemented\n"
            << "System access: No arbitrary commands\n"
            << "--------------------------------\n";
    }
};

int main() {
    try {
        Numerick assistant;
        assistant.run();
    }
    catch (const std::exception& error) {
        std::cerr
            << "NUMERICK error: "
            << error.what() << '\n';

        return 1;
    }

    return 0;
}
