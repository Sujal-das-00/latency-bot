#pragma once

#include "latency_arb/Types.hpp"

namespace latency_arb {

class TradeValidator {
public:
    explicit TradeValidator(Config config);

    [[nodiscard]] ValidationRecord validate(const Trade& trade, const MarketObservation& observation) const;

private:
    [[nodiscard]] double pnl_for_exit(const Trade& trade, double exit_price) const;
    [[nodiscard]] double markout_bps(const Trade& trade, double exit_price) const;
    [[nodiscard]] double entry_slippage_bps(const Trade& trade, const Book& latency_book) const;

    Config config_;
};

} // namespace latency_arb
