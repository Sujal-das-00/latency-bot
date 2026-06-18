#include "latency_arb/StatisticsEngine.hpp"

#include <utility>

namespace latency_arb {

void StatisticsEngine::record(const ValidationRecord& record)
{
    records_.push_back(record);
}

std::vector<ValidationRecord> StatisticsEngine::records() const
{
    return records_;
}

} // namespace latency_arb
