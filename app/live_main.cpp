#include "latency_arb/Config.hpp"
#include "latency_arb/CsvLogger.hpp"
#include "latency_arb/LiveMarketData.hpp"

#include <fmt/core.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage()
{
    fmt::print(
        "Usage: latency_arb_live --output=out-live [options]\n"
        "\n"
        "Public-market PAPER-ONLY live validator.\n"
        "No API keys, no auth websocket, no REST trading, no real orders.\n"
        "\n"
        "Options:\n"
        "  --coinbase-product=SOL-USDT\n"
        "  --phemex-symbol=sSOLUSDT\n"
        "  --duration-seconds=3600\n"
        "  --min-spread-bps=8\n"
        "  --min-lag-ms=50\n"
        "  --max-lag-ms=500\n"
        "  --assumed-latency-ms=25\n"
        "  --position-usdt=100\n"
        "  --fee-rate=0.0005\n");
}

[[nodiscard]] std::string value_after_equals(const std::string& arg, std::string_view key)
{
    if (!arg.starts_with(key) || arg.size() <= key.size() || arg[key.size()] != '=') {
        return {};
    }
    return arg.substr(key.size() + 1);
}

[[nodiscard]] std::uint64_t parse_u64(const std::string& value)
{
    std::uint64_t output{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument("invalid unsigned integer");
    }
    return output;
}

[[nodiscard]] std::string today_string()
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm_buf);
    return std::string(buffer);
}

// Route all spdlog output to both the console and a date-named log file so
// every "paper-only signal" line is persisted. Returns the log file path.
std::filesystem::path setup_logging(const std::filesystem::path& output_dir)
{
    const auto log_dir = output_dir / "logs";
    std::filesystem::create_directories(log_dir);
    const auto log_path = log_dir / fmt::format("signals_{}.log", today_string());

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), false);

    auto logger = std::make_shared<spdlog::logger>(
        "live", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);

    return log_path;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        latency_arb::LiveRunConfig live_config;
        for (const auto& arg : args) {
            if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            }
            if (const auto value = value_after_equals(arg, "--output"); !value.empty()) {
                live_config.output_dir = value;
            } else if (const auto value = value_after_equals(arg, "--coinbase-product"); !value.empty()) {
                live_config.coinbase_product = value;
            } else if (const auto value = value_after_equals(arg, "--phemex-symbol"); !value.empty()) {
                live_config.phemex_symbol = value;
            } else if (const auto value = value_after_equals(arg, "--duration-seconds"); !value.empty()) {
                live_config.duration_seconds = parse_u64(value);
            }
        }

        live_config.engine = latency_arb::ConfigParser::from_args(args);

        const auto log_path = setup_logging(live_config.output_dir);
        spdlog::info("logging signals to {}", log_path.string());

        latency_arb::LiveMarketDataRunner runner(live_config);
        const auto result = runner.run();

        latency_arb::CsvLogger logger(live_config.output_dir);
        logger.write_validation(result.records);
        logger.write_statistics(result.metrics);

        fmt::print(
            "signals={} wins={} losses={} winrate={:.4f} ev={:.8f} drawdown={:.8f} best_hold_ms={}\n",
            result.metrics.signals,
            result.metrics.wins,
            result.metrics.losses,
            result.metrics.winrate,
            result.metrics.expected_value,
            result.metrics.drawdown,
            result.metrics.best_hold_time_ms);
        fmt::print(
            "wrote {}/paper_validation.csv and {}/statistics.csv\n",
            live_config.output_dir.string(),
            live_config.output_dir.string());

        spdlog::info(
            "run complete signals={} wins={} losses={} winrate={:.4f} ev={:.8f} drawdown={:.8f} best_hold_ms={}",
            result.metrics.signals,
            result.metrics.wins,
            result.metrics.losses,
            result.metrics.winrate,
            result.metrics.expected_value,
            result.metrics.drawdown,
            result.metrics.best_hold_time_ms);
        return 0;
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return 1;
    }
}
