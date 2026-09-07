#ifndef AUTOCAN_CONSOLE_HPP
#define AUTOCAN_CONSOLE_HPP

#include <string>

namespace autocan {

class Console {
public:
    static void clear();
    static void title(const std::string& text);
    static void pause();
    static int readInt(const std::string& prompt, int minimum, int maximum);
    static void sleepMs(int milliseconds);
};

} // namespace autocan

#endif
