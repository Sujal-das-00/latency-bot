#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace latency_arb {

struct Book {
    double bid{};
    double ask{};
    double bid_qty{};
    double ask_qty{};
    std::uint64_t local_ts_ms{};
};

enum class Exchange {
    Reference,
    Target
};

enum class Direction {
    Buy,
    Sell
};

struct Config {
    std::string signal_exchange{"Coinbase"};
    std::string paper_exchange{"Phemex Spot"};
    std::string pair{"SOLUSDT"};

    double min_spread_bps{8.0};
    std::uint64_t min_lag_ms{50};
    std::uint64_t max_lag_ms{500};
    std::uint64_t assumed_latency_ms{25};
    double position_usdt{100.0};
    double fee_rate{0.0005};
    double entry_similarity_tolerance_bps{2.0};
    std::vector<std::uint64_t> observation_offsets_ms{
        25, 50, 100, 250, 500, 1000, 2000, 5000, 10000, 30000};
};

struct QuoteSnapshot {
    Book reference;
    Book target;
};

struct Signal {
    std::uint64_t id{};
    Direction direction{};
    double entry_bps{};
    std::uint64_t signal_ts{};
    std::uint64_t exchange_lag_ms{};
    Book coinbase;
    Book phemex;
};

enum class TradeState {
    PaperOnly
};

struct Trade {
    std::uint64_t id{};
    std::uint64_t signal_ts{};
    Direction direction{};
    double entry_bps{};
    std::uint64_t exchange_lag_ms{};
    double coinbase_bid{};
    double coinbase_ask{};
    double phemex_bid{};
    double phemex_ask{};
    double paper_entry_price{};
    std::uint64_t assumed_latency_ms{};
    TradeState state{TradeState::PaperOnly};
};

struct ObservationPoint {
    std::uint64_t offset_ms{};
    std::uint64_t ts_ms{};
    Book phemex;
    double exit_price{};
    bool available{};
};

struct MarketObservation {
    std::vector<ObservationPoint> points;
    std::optional<Book> latency_book;
};

struct ValidationRecord {
    Trade trade;
    std::map<std::uint64_t, double> observed_prices;
    double entry_similarity_bps{};
    double entry_slippage_bps{};
    double max_favorable_bps{};
    double max_adverse_bps{};
    std::uint64_t time_to_peak_ms{};
    std::uint64_t time_to_repricing_ms{};
    double best_exit_price{};
    std::uint64_t best_exit_ts{};
    double pnl_1s{};
    double pnl_2s{};
    double pnl_5s{};
    double pnl_10s{};
    double pnl_30s{};
    bool successful_prediction{};
    bool would_be_profitable{};
};

struct RiskMetrics {
    std::string date{"all"};
    std::uint64_t signals{};
    std::uint64_t wins{};
    std::uint64_t losses{};
    double signals_per_hour{};
    double prediction_accuracy{};
    double winrate{};
    double average_profit{};
    double average_loss{};
    double expected_value{};
    double sharpe{};
    double drawdown{};
    std::uint64_t maximum_losing_streak{};
    double average_hold_time_ms{};
    double median_hold_time_ms{};
    std::uint64_t best_hold_time_ms{};
};

[[nodiscard]] constexpr std::string_view to_string(Direction direction)
{
    switch (direction) {
    case Direction::Buy:
        return "BUY";
    case Direction::Sell:
        return "SELL";
    }
    return "UNKNOWN";
}

} // namespace latency_arb
