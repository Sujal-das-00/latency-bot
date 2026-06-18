#include "latency_arb/Config.hpp"

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace latency_arb {

namespace {

[[nodiscard]] bool parse_double(std::string_view value, double& output)
{
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool parse_u64(std::string_view value, std::uint64_t& output)
{
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] std::string_view value_after_equals(std::string_view arg, std::string_view key)
{
    if (!arg.starts_with(key) || arg.size() <= key.size() || arg[key.size()] != '=') {
        return {};
    }
    return arg.substr(key.size() + 1);
}

} // namespace

Config ConfigParser::from_args(const std::vector<std::string>& args)
{
    Config config;

    for (const auto& arg_string : args) {
        const std::string_view arg{arg_string};
        if (const auto value = value_after_equals(arg, "--min-spread-bps"); !value.empty()) {
            if (!parse_double(value, config.min_spread_bps)) {
                throw std::invalid_argument("invalid --min-spread-bps");
            }
        } else if (const auto value = value_after_equals(arg, "--min-lag-ms"); !value.empty()) {
            if (!parse_u64(value, config.min_lag_ms)) {
                throw std::invalid_argument("invalid --min-lag-ms");
            }
        } else if (const auto value = value_after_equals(arg, "--max-lag-ms"); !value.empty()) {
            if (!parse_u64(value, config.max_lag_ms)) {
                throw std::invalid_argument("invalid --max-lag-ms");
            }
        } else if (const auto value = value_after_equals(arg, "--assumed-latency-ms"); !value.empty()) {
            if (!parse_u64(value, config.assumed_latency_ms)) {
                throw std::invalid_argument("invalid --assumed-latency-ms");
            }
        } else if (const auto value = value_after_equals(arg, "--position-usdt"); !value.empty()) {
            if (!parse_double(value, config.position_usdt)) {
                throw std::invalid_argument("invalid --position-usdt");
            }
        } else if (const auto value = value_after_equals(arg, "--fee-rate"); !value.empty()) {
            if (!parse_double(value, config.fee_rate)) {
                throw std::invalid_argument("invalid --fee-rate");
            }
        } else if (const auto value = value_after_equals(arg, "--pair"); !value.empty()) {
            config.pair = std::string(value);
        }
    }

    if (config.min_lag_ms > config.max_lag_ms) {
        throw std::invalid_argument("--min-lag-ms cannot be greater than --max-lag-ms");
    }

    return config;
}

} // namespace latency_arb
