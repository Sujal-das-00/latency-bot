#include "latency_arb/Backtester.hpp"
#include "latency_arb/Config.hpp"
#include "latency_arb/CsvLogger.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage()
{
    fmt::print(
        "Usage: latency_arb_backtest --input=books.csv --output=out [options]\n"
        "\n"
        "This is a PAPER-ONLY spot lead-lag validator:\n"
        "  Coinbase is used only as the public signal source.\n"
        "  Phemex Spot is the only hypothetical paper trade venue.\n"
        "  No orders, auth APIs, hedges, shorts, or transfers exist in this binary.\n"
        "\n"
        "Input CSV columns:\n"
        "  exchange,bid,ask,bid_qty,ask_qty,local_ts_ms\n"
        "\n"
        "Options:\n"
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

} // namespace

int main(int argc, char** argv)
{
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        std::filesystem::path input_path;
        std::filesystem::path output_dir{"out"};
        for (const auto& arg : args) {
            if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            }
            if (const auto value = value_after_equals(arg, "--input"); !value.empty()) {
                input_path = value;
            } else if (const auto value = value_after_equals(arg, "--output"); !value.empty()) {
                output_dir = value;
            }
        }

        if (input_path.empty()) {
            print_usage();
            return 2;
        }

        auto config = latency_arb::ConfigParser::from_args(args);
        latency_arb::Backtester backtester(config);
        const auto result = backtester.run_csv(input_path);

        latency_arb::CsvLogger logger(output_dir);
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
        fmt::print("wrote {}/paper_validation.csv and {}/statistics.csv\n", output_dir.string(), output_dir.string());
        return 0;
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return 1;
    }
}
