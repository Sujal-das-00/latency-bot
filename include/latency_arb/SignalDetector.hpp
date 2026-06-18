#pragma once

#include "latency_arb/Types.hpp"

#include <array>
#include <optional>

namespace latency_arb {

class SignalDetector {
public:
    explicit SignalDetector(Config config);

    [[nodiscard]] std::vector<Signal> on_snapshot(const QuoteSnapshot& snapshot);
    [[nodiscard]] std::uint64_t signals_found() const;

    [[nodiscard]] static double buy_spread_bps(const QuoteSnapshot& snapshot);
    [[nodiscard]] static double sell_spread_bps(const QuoteSnapshot& snapshot);

private:
    [[nodiscard]] std::optional<Signal> maybe_signal(
        Direction direction,
        double spread_bps,
        const QuoteSnapshot& snapshot,
        std::uint64_t lag_ms);

    Config config_;
    std::uint64_t next_id_{1};
    std::uint64_t signals_found_{};
    std::array<bool, 2> active_{false, false};
};

} // namespace latency_arb
