#pragma once

#include "latency_arb/Backtester.hpp"
#include "latency_arb/Types.hpp"

#include <filesystem>
#include <string>

namespace latency_arb {

struct LiveRunConfig {
    Config engine;
    std::string coinbase_product{"SOL-USDT"};
    std::string phemex_symbol{"sSOLUSDT"};
    std::filesystem::path output_dir{"out-live"};
    std::uint64_t duration_seconds{3600};
};

class LiveMarketDataRunner {
public:
    explicit LiveMarketDataRunner(LiveRunConfig config);

    [[nodiscard]] BacktestResult run();

private:
    LiveRunConfig config_;
};

} // namespace latency_arb
