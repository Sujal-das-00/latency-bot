#pragma once

#include "latency_arb/Types.hpp"

namespace latency_arb {

class MarketObserver {
public:
    explicit MarketObserver(Config config);

    [[nodiscard]] MarketObservation observe(const Trade& trade, const std::vector<Book>& phemex_history) const;

private:
    [[nodiscard]] static std::optional<Book> book_at_or_after(
        const std::vector<Book>& history,
        std::uint64_t ts_ms);

    Config config_;
};

} // namespace latency_arb
