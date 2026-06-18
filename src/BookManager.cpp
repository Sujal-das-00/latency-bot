#include "latency_arb/BookManager.hpp"

#include <algorithm>

namespace latency_arb {

void BookManager::update(Exchange exchange, const Book& book)
{
    std::lock_guard lock(mutex_);
    if (exchange == Exchange::Reference) {
        latest_reference_ = book;
        reference_history_.push_back(book);
    } else {
        latest_target_ = book;
        target_history_.push_back(book);
    }
}

std::optional<QuoteSnapshot> BookManager::latest() const
{
    std::lock_guard lock(mutex_);
    if (!latest_reference_ || !latest_target_) {
        return std::nullopt;
    }
    return QuoteSnapshot{*latest_reference_, *latest_target_};
}

std::optional<QuoteSnapshot> BookManager::snapshot_at_or_before(std::uint64_t ts_ms) const
{
    std::lock_guard lock(mutex_);
    const auto reference = book_at_or_before(reference_history_, ts_ms);
    const auto target = book_at_or_before(target_history_, ts_ms);
    if (!reference || !target) {
        return std::nullopt;
    }
    return QuoteSnapshot{*reference, *target};
}

std::vector<Book> BookManager::target_history() const
{
    std::lock_guard lock(mutex_);
    return target_history_;
}

std::uint64_t BookManager::last_timestamp_ms() const
{
    std::lock_guard lock(mutex_);
    std::uint64_t last_ts = 0;
    if (latest_reference_) {
        last_ts = std::max(last_ts, latest_reference_->local_ts_ms);
    }
    if (latest_target_) {
        last_ts = std::max(last_ts, latest_target_->local_ts_ms);
    }
    return last_ts;
}

std::uint64_t BookManager::first_timestamp_ms() const
{
    std::lock_guard lock(mutex_);
    std::uint64_t first_ts = 0;
    if (!reference_history_.empty()) {
        first_ts = reference_history_.front().local_ts_ms;
    }
    if (!target_history_.empty()) {
        first_ts = first_ts == 0
                       ? target_history_.front().local_ts_ms
                       : std::min(first_ts, target_history_.front().local_ts_ms);
    }
    return first_ts;
}

std::optional<Book> BookManager::book_at_or_before(const std::vector<Book>& history, std::uint64_t ts_ms)
{
    const auto it = std::upper_bound(
        history.begin(),
        history.end(),
        ts_ms,
        [](std::uint64_t value, const Book& book) { return value < book.local_ts_ms; });

    if (it == history.begin()) {
        return std::nullopt;
    }

    return *(it - 1);
}

} // namespace latency_arb
