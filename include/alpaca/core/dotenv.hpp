#pragma once

#include <string_view>

namespace alpaca::core {

// Loads KEY=VALUE pairs from a .env-style file into the process environment.
// Returns true if the file was found and parsed successfully.
bool load_env_file(std::string_view path = ".env");

}  // namespace alpaca::core



