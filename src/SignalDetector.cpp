#include "latency_arb/SignalDetector.hpp"

#include <algorithm>
#include <cmath>

namespace latency_arb {

namespace {

[[nodiscard]] std::size_t index_for(Direction direction)
{
    return direction == Direction::Buy ? 0U : 1U;
}

[[nodiscard]] std::uint64_t lag_ms(const QuoteSnapshot& snapshot)
{
    if (snapshot.reference.local_ts_ms <= snapshot.target.local_ts_ms) {
        return 0;
    }
    return snapshot.reference.local_ts_ms - snapshot.target.local_ts_ms;
}

} // namespace

SignalDetector::SignalDetector(Config config)
    : config_(std::move(config))
{
}

std::vector<Signal> SignalDetector::on_snapshot(const QuoteSnapshot& snapshot)
{
    std::vector<Signal> signals;
    const auto lag = lag_ms(snapshot);

    if (auto signal = maybe_signal(Direction::Buy, buy_spread_bps(snapshot), snapshot, lag)) {
        signals.push_back(*signal);
    }
    if (auto signal = maybe_signal(Direction::Sell, sell_spread_bps(snapshot), snapshot, lag)) {
        signals.push_back(*signal);
    }

    return signals;
}

std::uint64_t SignalDetector::signals_found() const
{
    return signals_found_;
}

double SignalDetector::buy_spread_bps(const QuoteSnapshot& snapshot)
{
    return ((snapshot.reference.bid - snapshot.target.ask) /
            ((snapshot.reference.bid + snapshot.target.ask) / 2.0)) *
           10000.0;
}

double SignalDetector::sell_spread_bps(const QuoteSnapshot& snapshot)
{
    return ((snapshot.target.bid - snapshot.reference.ask) /
            ((snapshot.target.bid + snapshot.reference.ask) / 2.0)) *
           10000.0;
}

std::optional<Signal> SignalDetector::maybe_signal(
    Direction direction,
    double spread_bps,
    const QuoteSnapshot& snapshot,
    std::uint64_t exchange_lag_ms)
{
    const auto index = index_for(direction);
    const auto lag_ok = exchange_lag_ms >= config_.min_lag_ms && exchange_lag_ms <= config_.max_lag_ms;
    const auto spread_ok = spread_bps >= config_.min_spread_bps;

    if (!lag_ok || !spread_ok) {
        active_[index] = false;
        return std::nullopt;
    }

    if (active_[index]) {
        return std::nullopt;
    }

    active_[index] = true;
    ++signals_found_;
    return Signal{
        .id = next_id_++,
        .direction = direction,
        .entry_bps = spread_bps,
        .signal_ts = snapshot.reference.local_ts_ms,
        .exchange_lag_ms = exchange_lag_ms,
        .coinbase = snapshot.reference,
        .phemex = snapshot.target};
}

} // namespace latency_arb
