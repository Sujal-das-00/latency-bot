#include "latency_arb/Backtester.hpp"
#include "latency_arb/MarketObserver.hpp"
#include "latency_arb/PaperTrader.hpp"
#include "latency_arb/SignalDetector.hpp"
#include "latency_arb/TradeValidator.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

latency_arb::Config test_config()
{
    latency_arb::Config config;
    config.min_spread_bps = 8.0;
    config.min_lag_ms = 50;
    config.max_lag_ms = 500;
    config.assumed_latency_ms = 25;
    config.position_usdt = 100.0;
    config.fee_rate = 0.0005;
    return config;
}

void detector_emits_buy_signal_when_coinbase_leads_up()
{
    auto config = test_config();
    latency_arb::SignalDetector detector(config);

    const latency_arb::QuoteSnapshot snapshot{
        .reference = latency_arb::Book{.bid = 100.20, .ask = 100.30, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1100},
        .target = latency_arb::Book{.bid = 99.80, .ask = 100.00, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1000}};

    const auto signals = detector.on_snapshot(snapshot);
    assert(signals.size() == 1);
    assert(signals.front().direction == latency_arb::Direction::Buy);
    assert(signals.front().exchange_lag_ms == 100);
    assert(signals.front().entry_bps > config.min_spread_bps);
}

void paper_trader_uses_only_phemex_entry_price()
{
    auto config = test_config();
    latency_arb::PaperTrader trader(config);
    const latency_arb::Signal signal{
        .id = 42,
        .direction = latency_arb::Direction::Buy,
        .entry_bps = 12.0,
        .signal_ts = 1100,
        .exchange_lag_ms = 100,
        .coinbase = latency_arb::Book{.bid = 100.20, .ask = 100.30, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1100},
        .phemex = latency_arb::Book{.bid = 99.80, .ask = 100.00, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1000}};

    const auto trade = trader.create_trade(signal);
    assert(trade.paper_entry_price == 100.00);
    assert(trade.coinbase_bid == 100.20);
    assert(trade.phemex_ask == 100.00);
}

void validator_calculates_future_phemex_markouts()
{
    auto config = test_config();
    latency_arb::Trade trade{
        .id = 1,
        .signal_ts = 1000,
        .direction = latency_arb::Direction::Buy,
        .entry_bps = 20.0,
        .exchange_lag_ms = 100,
        .coinbase_bid = 100.20,
        .coinbase_ask = 100.30,
        .phemex_bid = 99.80,
        .phemex_ask = 100.00,
        .paper_entry_price = 100.00,
        .assumed_latency_ms = 25};
    const std::vector<latency_arb::Book> phemex_history{
        latency_arb::Book{.bid = 99.80, .ask = 100.00, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1000},
        latency_arb::Book{.bid = 100.05, .ask = 100.10, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1025},
        latency_arb::Book{.bid = 100.30, .ask = 100.35, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 2000},
        latency_arb::Book{.bid = 100.40, .ask = 100.45, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 30000}};

    latency_arb::MarketObserver observer(config);
    latency_arb::TradeValidator validator(config);
    const auto record = validator.validate(trade, observer.observe(trade, phemex_history));

    assert(record.entry_similarity_bps > 0.0);
    assert(record.max_favorable_bps > 0.0);
    assert(record.pnl_1s > 0.0);
    assert(record.successful_prediction);
}

void backtester_replays_and_validates_signal()
{
    auto config = test_config();
    latency_arb::Backtester backtester(config);

    backtester.on_book(latency_arb::Exchange::Target, latency_arb::Book{.bid = 99.80, .ask = 100.00, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1000});
    backtester.on_book(latency_arb::Exchange::Reference, latency_arb::Book{.bid = 100.20, .ask = 100.30, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 1100});
    backtester.on_book(latency_arb::Exchange::Target, latency_arb::Book{.bid = 100.25, .ask = 100.30, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 2100});
    backtester.on_book(latency_arb::Exchange::Target, latency_arb::Book{.bid = 100.40, .ask = 100.45, .bid_qty = 5.0, .ask_qty = 5.0, .local_ts_ms = 31100});

    const auto result = backtester.finalize();
    assert(result.metrics.signals == 1);
    assert(result.records.size() == 1);
    assert(result.records.front().trade.direction == latency_arb::Direction::Buy);
    assert(result.records.front().pnl_1s > 0.0);
}

} // namespace

int main()
{
    detector_emits_buy_signal_when_coinbase_leads_up();
    paper_trader_uses_only_phemex_entry_price();
    validator_calculates_future_phemex_markouts();
    backtester_replays_and_validates_signal();
    return 0;
}
