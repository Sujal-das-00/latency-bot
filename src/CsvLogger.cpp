#include "latency_arb/CsvLogger.hpp"

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace latency_arb {

namespace {

[[nodiscard]] double observed_price(const ValidationRecord& record, std::uint64_t offset)
{
    const auto it = record.observed_prices.find(offset);
    return it == record.observed_prices.end() ? 0.0 : it->second;
}

[[nodiscard]] int bool_int(bool value)
{
    return value ? 1 : 0;
}

} // namespace

CsvLogger::CsvLogger(std::filesystem::path output_dir)
    : output_dir_(std::move(output_dir))
{
}

void CsvLogger::write_validation(const std::vector<ValidationRecord>& records) const
{
    std::filesystem::create_directories(output_dir_);
    std::ofstream output(output_dir_ / "paper_validation.csv");
    if (!output) {
        throw std::runtime_error("failed to open paper_validation.csv");
    }

    output << "trade_id,signal_ts,direction,entry_bps,exchange_lag_ms,coinbase_bid,coinbase_ask,"
              "phemex_bid,phemex_ask,paper_entry_price,assumed_latency_ms,price_25ms,price_50ms,"
              "price_100ms,price_250ms,price_500ms,price_1s,price_2s,price_5s,price_10s,"
              "price_30s,max_favorable_bps,max_adverse_bps,best_exit_price,best_exit_ts,"
              "pnl_1s,pnl_2s,pnl_5s,pnl_10s,pnl_30s,successful_prediction,would_be_profitable\n";

    for (const auto& record : records) {
        const auto& trade = record.trade;
        output << fmt::format(
            "{},{},{},{:.8f},{},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{},"
            "{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},"
            "{:.8f},{:.8f},{:.8f},{},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{},{}\n",
            trade.id,
            trade.signal_ts,
            to_string(trade.direction),
            trade.entry_bps,
            trade.exchange_lag_ms,
            trade.coinbase_bid,
            trade.coinbase_ask,
            trade.phemex_bid,
            trade.phemex_ask,
            trade.paper_entry_price,
            trade.assumed_latency_ms,
            observed_price(record, 25),
            observed_price(record, 50),
            observed_price(record, 100),
            observed_price(record, 250),
            observed_price(record, 500),
            observed_price(record, 1000),
            observed_price(record, 2000),
            observed_price(record, 5000),
            observed_price(record, 10000),
            observed_price(record, 30000),
            record.max_favorable_bps,
            record.max_adverse_bps,
            record.best_exit_price,
            record.best_exit_ts,
            record.pnl_1s,
            record.pnl_2s,
            record.pnl_5s,
            record.pnl_10s,
            record.pnl_30s,
            bool_int(record.successful_prediction),
            bool_int(record.would_be_profitable));
    }
}

void CsvLogger::write_statistics(const RiskMetrics& metrics) const
{
    std::filesystem::create_directories(output_dir_);
    std::ofstream output(output_dir_ / "statistics.csv");
    if (!output) {
        throw std::runtime_error("failed to open statistics.csv");
    }

    output << "date,signals,wins,losses,winrate,avg_trade,expected_value,sharpe,drawdown,best_hold_time\n";
    output << fmt::format(
        "{},{},{},{},{:.8f},{:.8f},{:.8f},{:.8f},{:.8f},{}\n",
        metrics.date,
        metrics.signals,
        metrics.wins,
        metrics.losses,
        metrics.winrate,
        metrics.expected_value,
        metrics.expected_value,
        metrics.sharpe,
        metrics.drawdown,
        metrics.best_hold_time_ms);
}

} // namespace latency_arb
