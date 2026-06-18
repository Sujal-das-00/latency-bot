#pragma once

#include "latency_arb/Types.hpp"

namespace latency_arb {

class RiskAnalyzer {
public:
    explicit RiskAnalyzer(Config config);

    [[nodiscard]] RiskMetrics analyze(const std::vector<ValidationRecord>& records) const;

private:
    Config config_;
};

} // namespace latency_arb
