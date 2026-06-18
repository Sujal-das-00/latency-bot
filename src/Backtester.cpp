#include "latency_arb/Backtester.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace latency_arb {

namespace {

[[nodiscard]] std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::vector<std::string> split_csv_line(const std::string& line)
{
    std::vector<std::string> cells;
    std::stringstream stream(line);
    std::string cell;
    while (std::getline(stream, cell, ',')) {
        cells.push_back(trim(cell));
    }
    return cells;
}

[[nodiscard]] double parse_double(const std::string& value, std::string_view column)
{
    double output{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(fmt::format("invalid numeric value for {}", column));
    }
    return output;
}

[[nodiscard]] std::uint64_t parse_u64(const std::string& value, std::string_view column)
{
    std::uint64_t output{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(fmt::format("invalid integer value for {}", column));
    }
    return output;
}

[[nodiscard]] Exchange parse_exchange(const std::string& value)
{
    if (value == "Coinbase" || value == "coinbase" || value == "signal") {
        return Exchange::Reference;
    }
    if (value == "Phemex" || value == "phemex" || value == "Phemex Spot" || value == "paper") {
        return Exchange::Target;
    }
    throw std::invalid_argument("exchange must be Coinbase/signal or Phemex/paper");
}

[[nodiscard]] bool looks_like_header(const std::vector<std::string>& cells)
{
    return !cells.empty() && (cells.front() == "exchange" || cells.front() == "Exchange");
}

} // namespace

Backtester::Backtester(Config config)
    : config_(std::move(config))
    , detector_(config_)
    , paper_trader_(config_)
    , market_observer_(config_)
    , validator_(config_)
    , risk_analyzer_(config_)
{
}

Backtester::~Backtester()
{
    stop();
}

void Backtester::on_book(Exchange exchange, const Book& book)
{
    std::lock_guard lock(mutex_);
    if (book.bid <= 0.0 || book.ask <= 0.0 || book.bid > book.ask) {
        spdlog::warn("dropping invalid book: bid={} ask={} ts={}", book.bid, book.ask, book.local_ts_ms);
        return;
    }

    book_manager_.update(exchange, book);
    const auto snapshot = book_manager_.latest();
    if (!snapshot) {
        return;
    }

    process_signals(detector_.on_snapshot(*snapshot));
}

BacktestResult Backtester::finalize()
{
    std::lock_guard lock(mutex_);
    const auto phemex_history = book_manager_.target_history();
    std::vector<ValidationRecord> records;
    records.reserve(paper_trades_.size());

    for (const auto& trade : paper_trades_) {
        const auto observation = market_observer_.observe(trade, phemex_history);
        records.push_back(validator_.validate(trade, observation));
    }

    return BacktestResult{
        .metrics = risk_analyzer_.analyze(records),
        .records = records};
}

void Backtester::start()
{
    if (worker_) {
        return;
    }
    worker_.emplace([this](std::stop_token stop_token) { worker(stop_token); });
}

void Backtester::stop()
{
    if (!worker_) {
        return;
    }
    worker_->request_stop();
    queue_.notify_all();
    worker_.reset();
}

void Backtester::enqueue(BookUpdate update)
{
    queue_.push(std::move(update));
}

BacktestResult Backtester::run_csv(const std::filesystem::path& input_path)
{
    std::ifstream input(input_path);
    if (!input) {
        throw std::runtime_error(fmt::format("failed to open input CSV: {}", input_path.string()));
    }

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }

        const auto cells = split_csv_line(line);
        if (line_number == 1 && looks_like_header(cells)) {
            continue;
        }
        if (cells.size() != 6) {
            throw std::invalid_argument(fmt::format(
                "line {} must have 6 columns: exchange,bid,ask,bid_qty,ask_qty,local_ts_ms",
                line_number));
        }

        on_book(
            parse_exchange(cells[0]),
            Book{
                .bid = parse_double(cells[1], "bid"),
                .ask = parse_double(cells[2], "ask"),
                .bid_qty = parse_double(cells[3], "bid_qty"),
                .ask_qty = parse_double(cells[4], "ask_qty"),
                .local_ts_ms = parse_u64(cells[5], "local_ts_ms")});
    }

    return finalize();
}

void Backtester::process_signals(const std::vector<Signal>& signals)
{
    for (const auto& signal : signals) {
        auto trade = paper_trader_.create_trade(signal);
        spdlog::info(
            "paper-only signal id={} direction={} entry_bps={:.4f} lag_ms={} phemex_entry={:.8f}",
            trade.id,
            to_string(trade.direction),
            trade.entry_bps,
            trade.exchange_lag_ms,
            trade.paper_entry_price);
        paper_trades_.push_back(trade);
    }
}

void Backtester::worker(std::stop_token stop_token)
{
    while (!stop_token.stop_requested()) {
        auto update = queue_.wait_pop(stop_token);
        if (!update) {
            continue;
        }
        on_book(update->exchange, update->book);
    }
}

} // namespace latency_arb
