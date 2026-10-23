
#include "numerick/ai_client.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
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

            const std::string command = normalize(input);

            if (command.empty()) {
                continue;
            }

            // Do not write AI prompt contents to the activity log.
            if (command.rfind("ask ", 0) == 0) {
                log("AI request submitted");
            } else {
                log("Input received: " + command);
            }

            if (command == "exit" || command == "quit") {
                std::cout
                    << "NUMERICK > Shutting down. Goodbye.\n";
                break;
            }

            handleCommand(input);
        }
    }

private:
    std::ofstream logFile;
    numerick::AIClient aiClient;

    static std::string trim(const std::string& text) {
        const auto first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos) {
            return "";
        }

        const auto last = text.find_last_not_of(" \t\r\n");

        return text.substr(first, last - first + 1);
    }

    static std::string normalize(std::string text) {
        text = trim(text);

        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        return text;
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
            << "Core version: 0.2.0\n"
            << "AI model: Qwen 3.5 4B via Ollama\n"
            << "Type 'help' to see available commands.\n";
    }

    void handleCommand(const std::string& input) {
        const std::string command = normalize(input);

        if (command == "help" || command == "commands") {
            showHelp();
        }
        else if (command == "hello" ||
                 command == "hi" ||
                 command == "hey") {
            std::cout
                << "NUMERICK > Hello. I'm online and ready.\n";
        }
        else if (command == "status") {
            showStatus();
        }
        else if (command == "time") {
            std::cout
                << "NUMERICK > Local time: "
                << currentTime() << '\n';
        }
        else if (command == "clear") {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            printWelcome();
        }
        else if (command.rfind("ask ", 0) == 0) {
            const std::string cleaned = trim(input);
            const std::string prompt = trim(cleaned.substr(4));

            if (prompt.empty()) {
                std::cout
                    << "NUMERICK > Usage: ask <your question>\n";
                return;
            }

            std::cout << "NUMERICK > Thinking...\n";

            try {
                const std::string answer = aiClient.ask(prompt);

                std::cout
                    << "\nNUMERICK > "
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
        }
        else {
            std::cout
                << "NUMERICK > I don't understand that command "
                << "yet. Try 'help' or 'ask <question>'.\n";

            log("Unsupported command: " + command);
        }
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
        std::cerr
            << "NUMERICK error: "
            << error.what() << '\n';

        return 1;
    }

    return 0;
}
