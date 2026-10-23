#include "numerick/ai_client.hpp"
#include "numerick/command_parser.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

class Numerick {
public:
    Numerick() {
        fs::create_directories("data");
        logFile.open("data/activity.log", std::ios::app);
        log("NUMERICK started");
    }

    ~Numerick() {
        log("NUMERICK stopped");

        if (logFile.is_open()) {
            logFile.close();
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

            // Avoid storing the contents of AI prompts in the log.
            if (command.type == numerick::CommandType::Ask) {
                log("AI request submitted");
            } else {
                log("Input received: " + input);
            }

            if (!handleCommand(command)) {
                break;
            }
        }
    }

private:
    std::ofstream logFile;
    numerick::AIClient aiClient;

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
        if (logFile.is_open()) {
            logFile << "[" << currentTime() << "] "
                    << message << '\n';
            logFile.flush();
        }
    }

    void printWelcome() const {
        std::cout
            << "\n"
            << "====================================\n"
            << "          N U M E R I C K\n"
            << "       Personal Assistant Core\n"
            << "====================================\n"
            << "Core version: 0.3.0\n"
            << "AI model: Qwen 3.5 4B via Ollama\n"
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
                const std::string answer =
                    aiClient.ask(command.argument);

                std::cout << "\nNUMERICK > "
                          << answer << '\n';

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

        case CommandType::Exit:
            std::cout
                << "NUMERICK > Shutting down. Goodbye.\n";
            return false;

        case CommandType::Unknown:
            std::cout
                << "NUMERICK > I don't recognize that command "
                << "yet. Try 'help' or 'ask <question>'.\n";

            log("Unsupported command: " + command.argument);
            break;
        }

        return true;
    }

    void showHelp() const {
        std::cout
            << "\nAvailable commands:\n"
            << "  help             - Show available commands\n"
            << "  hello            - Test the assistant\n"
            << "  status           - Show current status\n"
            << "  time             - Display local time\n"
            << "  ask <question>   - Ask the local AI model\n"
            << "  clear            - Clear the console\n"
            << "  exit             - Shut down NUMERICK\n"
            << "\n";
    }

    void showStatus() const {
        std::cout
            << "\nNUMERICK STATUS\n"
            << "--------------------------------\n"
            << "Core:          Running\n"
            << "AI model:      Qwen 3.5 4B\n"
            << "AI runtime:    Local Ollama API\n"
            << "Voice input:   Not implemented\n"
            << "Vision:        Not implemented\n"
            << "Memory:        Basic activity log\n"
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
        std::cerr << "NUMERICK error: "
                  << error.what() << '\n';

        return 1;
    }

    return 0;
}
