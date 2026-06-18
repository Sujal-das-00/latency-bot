#pragma once

#include "latency_arb/Types.hpp"

#include <filesystem>

namespace latency_arb {

class CsvLogger {
public:
    explicit CsvLogger(std::filesystem::path output_dir);

    void write_validation(const std::vector<ValidationRecord>& records) const;
    void write_statistics(const RiskMetrics& metrics) const;

private:
    std::filesystem::path output_dir_;
};

} // namespace latency_arb
