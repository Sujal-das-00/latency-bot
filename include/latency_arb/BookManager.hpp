#pragma once

#include "latency_arb/Types.hpp"

#include <mutex>
#include <optional>
#include <vector>

namespace latency_arb {

class BookManager {
public:
    void update(Exchange exchange, const Book& book);

    [[nodiscard]] std::optional<QuoteSnapshot> latest() const;
    [[nodiscard]] std::optional<QuoteSnapshot> snapshot_at_or_before(std::uint64_t ts_ms) const;
    [[nodiscard]] std::vector<Book> target_history() const;
    [[nodiscard]] std::uint64_t last_timestamp_ms() const;
    [[nodiscard]] std::uint64_t first_timestamp_ms() const;

private:
    static std::optional<Book> book_at_or_before(const std::vector<Book>& history, std::uint64_t ts_ms);

    mutable std::mutex mutex_;
    std::optional<Book> latest_reference_;
    std::optional<Book> latest_target_;
    std::vector<Book> reference_history_;
    std::vector<Book> target_history_;
};

} // namespace latency_arb
