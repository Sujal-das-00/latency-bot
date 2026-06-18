#include "latency_arb/PaperTrader.hpp"

#include <utility>

namespace latency_arb {

PaperTrader::PaperTrader(Config config)
    : config_(std::move(config))
{
}

Trade PaperTrader::create_trade(const Signal& signal) const
{
    const auto paper_entry_price = signal.direction == Direction::Buy
                                       ? signal.phemex.ask
                                       : signal.phemex.bid;

    return Trade{
        .id = signal.id,
        .signal_ts = signal.signal_ts,
        .direction = signal.direction,
        .entry_bps = signal.entry_bps,
        .exchange_lag_ms = signal.exchange_lag_ms,
        .coinbase_bid = signal.coinbase.bid,
        .coinbase_ask = signal.coinbase.ask,
        .phemex_bid = signal.phemex.bid,
        .phemex_ask = signal.phemex.ask,
        .paper_entry_price = paper_entry_price,
        .assumed_latency_ms = config_.assumed_latency_ms,
        .state = TradeState::PaperOnly};
}

} // namespace latency_arb
