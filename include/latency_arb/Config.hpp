#pragma once

#include "latency_arb/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace latency_arb {

class ConfigParser {
public:
    [[nodiscard]] static Config from_args(const std::vector<std::string>& args);
};

} // namespace latency_arb
