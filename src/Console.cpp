#include "autocan/Console.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

namespace autocan {

void Console::clear() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void Console::title(const std::string& text) {
    std::cout << std::string(100, '=') << '\n'
              << text << '\n'
              << std::string(100, '=') << '\n';
}

void Console::pause() {
    std::cout << "\nPress Enter to continue...";
    std::string input;
    std::getline(std::cin, input);
}

int Console::readInt(const std::string& prompt, int minimum, int maximum) {
    for (;;) {
        std::cout << prompt;
        std::string input;

        if (!std::getline(std::cin, input)) {
            return minimum;
        }

        std::istringstream inputStream(input);
        int value = 0;
        char extra = 0;

        if ((inputStream >> value) &&
            !(inputStream >> extra) &&
            value >= minimum && value <= maximum) {
            return value;
        }

        std::cout << "Invalid choice. Enter " << minimum
                  << " to " << maximum << ".\n";
    }
}

void Console::sleepMs(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace autocan
