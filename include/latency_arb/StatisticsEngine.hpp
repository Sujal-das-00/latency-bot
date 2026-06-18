#pragma once

#include "latency_arb/Types.hpp"

#include <vector>

namespace latency_arb {

class StatisticsEngine {
public:
    void record(const ValidationRecord& record);

    [[nodiscard]] std::vector<ValidationRecord> records() const;

private:
    std::vector<ValidationRecord> records_;
};

} // namespace latency_arb
