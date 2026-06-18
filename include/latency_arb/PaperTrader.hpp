#pragma once

#include "latency_arb/Types.hpp"

namespace latency_arb {

class PaperTrader {
public:
    explicit PaperTrader(Config config);

    [[nodiscard]] Trade create_trade(const Signal& signal) const;

private:
    Config config_;
};

} // namespace latency_arb
