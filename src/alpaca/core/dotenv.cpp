#include "alpaca/core/dotenv.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace alpaca::core {

bool load_env_file(std::string_view path) {
    std::ifstream file{std::string(path)};
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        if (!key.empty()) {
#if defined(_WIN32)
            _putenv((key + "=" + value).c_str());
#else
            setenv(key.c_str(), value.c_str(), 1);
#endif
        }
    }
    return true;
}

}  // namespace alpaca::core


