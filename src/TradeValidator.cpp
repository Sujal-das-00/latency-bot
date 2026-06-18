#include "latency_arb/TradeValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace latency_arb {

TradeValidator::TradeValidator(Config config)
    : config_(std::move(config))
{
}

ValidationRecord TradeValidator::validate(const Trade& trade, const MarketObservation& observation) const
{
    ValidationRecord record;
    record.trade = trade;

    double best_favorable = -std::numeric_limits<double>::infinity();
    double worst_adverse = 0.0;
    bool repriced = false;

    for (const auto& point : observation.points) {
        if (!point.available) {
            continue;
        }

        record.observed_prices[point.offset_ms] = point.exit_price;
        const auto markout = markout_bps(trade, point.exit_price);
        if (markout > best_favorable) {
            best_favorable = markout;
            record.best_exit_price = point.exit_price;
            record.best_exit_ts = point.ts_ms;
            record.time_to_peak_ms = point.offset_ms;
        }
        if (markout < worst_adverse) {
            worst_adverse = markout;
        }
        if (!repriced && markout > 0.0) {
            record.time_to_repricing_ms = point.offset_ms;
            repriced = true;
        }

        const auto pnl = pnl_for_exit(trade, point.exit_price);
        if (point.offset_ms == 1000) {
            record.pnl_1s = pnl;
        } else if (point.offset_ms == 2000) {
            record.pnl_2s = pnl;
        } else if (point.offset_ms == 5000) {
            record.pnl_5s = pnl;
        } else if (point.offset_ms == 10000) {
            record.pnl_10s = pnl;
        } else if (point.offset_ms == 30000) {
            record.pnl_30s = pnl;
        }
    }

    record.max_favorable_bps = std::isfinite(best_favorable) ? std::max(0.0, best_favorable) : 0.0;
    record.max_adverse_bps = std::abs(worst_adverse);

    if (observation.latency_book) {
        const auto latency_entry = trade.direction == Direction::Buy
                                       ? observation.latency_book->ask
                                       : observation.latency_book->bid;
        record.entry_similarity_bps =
            std::abs((latency_entry - trade.paper_entry_price) / trade.paper_entry_price) * 10000.0;
        record.entry_slippage_bps = entry_slippage_bps(trade, *observation.latency_book);
    }

    record.successful_prediction = record.max_favorable_bps > 0.0;
    record.would_be_profitable = record.pnl_30s > 0.0;

    return record;
}

double TradeValidator::pnl_for_exit(const Trade& trade, double exit_price) const
{
    if (trade.paper_entry_price <= 0.0 || exit_price <= 0.0) {
        return 0.0;
    }

    const auto base_qty = config_.position_usdt / trade.paper_entry_price;
    const auto gross = trade.direction == Direction::Buy
                           ? (exit_price - trade.paper_entry_price) * base_qty
                           : (trade.paper_entry_price - exit_price) * base_qty;
    const auto fees = config_.position_usdt * config_.fee_rate + (exit_price * base_qty) * config_.fee_rate;
    return gross - fees;
}

double TradeValidator::markout_bps(const Trade& trade, double exit_price) const
{
    if (trade.paper_entry_price <= 0.0 || exit_price <= 0.0) {
        return 0.0;
    }
    if (trade.direction == Direction::Buy) {
        return ((exit_price - trade.paper_entry_price) / trade.paper_entry_price) * 10000.0;
    }
    return ((trade.paper_entry_price - exit_price) / trade.paper_entry_price) * 10000.0;
}

double TradeValidator::entry_slippage_bps(const Trade& trade, const Book& latency_book) const
{
    const auto latency_entry = trade.direction == Direction::Buy ? latency_book.ask : latency_book.bid;
    if (trade.paper_entry_price <= 0.0 || latency_entry <= 0.0) {
        return 0.0;
    }
    if (trade.direction == Direction::Buy) {
        return std::max(0.0, ((latency_entry - trade.paper_entry_price) / trade.paper_entry_price) * 10000.0);
    }
    return std::max(0.0, ((trade.paper_entry_price - latency_entry) / trade.paper_entry_price) * 10000.0);
}

} // namespace latency_arb
