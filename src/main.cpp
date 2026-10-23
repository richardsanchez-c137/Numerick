
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

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

            input = normalize(input);

            if (input.empty()) {
                continue;
            }

            log("Input received: " + input);

            if (input == "exit" || input == "quit") {
                std::cout << "NUMERICK > Shutting down. Goodbye.\n";
                break;
            }

            handleCommand(input);
        }
    }

private:
    std::ofstream logFile;

    static std::string normalize(std::string text) {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        const auto first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos) {
            return "";
        }

        const auto last = text.find_last_not_of(" \t\r\n");

        return text.substr(first, last - first + 1);
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
            << "Core version: 0.1.0\n"
            << "AI model: Not connected yet\n"
            << "Type 'help' to see available commands.\n";
    }

    void handleCommand(const std::string& input) {
        if (input == "help" || input == "commands") {
            showHelp();
        }
        else if (input == "hello" || input == "hi"
                 || input == "hey") {
            std::cout
                << "NUMERICK > Hello. I'm online and ready.\n";
        }
        else if (input == "status") {
            showStatus();
        }
        else if (input == "time") {
            std::cout
                << "NUMERICK > Local time: "
                << currentTime() << '\n';
        }
        else if (input == "clear") {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            printWelcome();
        }
        else {
            std::cout
                << "NUMERICK > I don't understand that command "
                << "yet. My AI reasoning module isn't connected.\n";

            log("Unsupported command: " + input);
        }
    }

    void showHelp() const {
        std::cout
            << "\nAvailable commands:\n"
            << "  help      - Show available commands\n"
            << "  hello     - Test the assistant\n"
            << "  status    - Show system status\n"
            << "  time      - Display local time\n"
            << "  clear     - Clear the console\n"
            << "  exit      - Shut down NUMERICK\n"
            << "\n";
    }

    void showStatus() const {
        std::cout
            << "\nNUMERICK STATUS\n"
            << "-------------------------------\n"
            << "Core:          Running\n"
            << "Language:      C++17\n"
            << "AI model:      Not connected\n"
            << "Voice input:   Not implemented\n"
            << "Vision:        Not implemented\n"
            << "Memory:        Basic activity log\n"
            << "System access: No arbitrary commands\n"
            << "-------------------------------\n";
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
