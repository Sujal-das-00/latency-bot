#pragma once

#include "latency_arb/BookManager.hpp"
#include "latency_arb/MarketObserver.hpp"
#include "latency_arb/PaperTrader.hpp"
#include "latency_arb/RiskAnalyzer.hpp"
#include "latency_arb/SignalDetector.hpp"
#include "latency_arb/ThreadSafeQueue.hpp"
#include "latency_arb/TradeValidator.hpp"
#include "latency_arb/Types.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace latency_arb {

struct BookUpdate {
    Exchange exchange{};
    Book book;
};

struct BacktestResult {
    RiskMetrics metrics;
    std::vector<ValidationRecord> records;
};

class Backtester {
public:
    explicit Backtester(Config config);
    ~Backtester();

    void on_book(Exchange exchange, const Book& book);
    [[nodiscard]] BacktestResult finalize();

    void start();
    void stop();
    void enqueue(BookUpdate update);

    [[nodiscard]] BacktestResult run_csv(const std::filesystem::path& input_path);

private:
    void process_signals(const std::vector<Signal>& signals);
    void worker(std::stop_token stop_token);

    Config config_;
    std::mutex mutex_;
    BookManager book_manager_;
    SignalDetector detector_;
    PaperTrader paper_trader_;
    MarketObserver market_observer_;
    TradeValidator validator_;
    RiskAnalyzer risk_analyzer_;
    std::vector<Trade> paper_trades_;
    ThreadSafeQueue<BookUpdate> queue_;
    std::optional<std::jthread> worker_;
};

} // namespace latency_arb
