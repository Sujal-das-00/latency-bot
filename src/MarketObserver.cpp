#include "latency_arb/MarketObserver.hpp"

#include <algorithm>
#include <utility>

namespace latency_arb {

MarketObserver::MarketObserver(Config config)
    : config_(std::move(config))
{
}

MarketObservation MarketObserver::observe(const Trade& trade, const std::vector<Book>& phemex_history) const
{
    MarketObservation observation;
    observation.latency_book = book_at_or_after(phemex_history, trade.signal_ts + config_.assumed_latency_ms);

    for (const auto offset : config_.observation_offsets_ms) {
        ObservationPoint point;
        point.offset_ms = offset;
        point.ts_ms = trade.signal_ts + offset;

        if (auto book = book_at_or_after(phemex_history, point.ts_ms)) {
            point.phemex = *book;
            point.exit_price = trade.direction == Direction::Buy ? book->bid : book->ask;
            point.available = true;
        }

        observation.points.push_back(point);
    }

    return observation;
}

std::optional<Book> MarketObserver::book_at_or_after(const std::vector<Book>& history, std::uint64_t ts_ms)
{
    const auto it = std::lower_bound(
        history.begin(),
        history.end(),
        ts_ms,
        [](const Book& book, std::uint64_t value) { return book.local_ts_ms < value; });

    if (it == history.end()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace latency_arb
