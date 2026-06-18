#include "latency_arb/RiskAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

namespace latency_arb {

namespace {

[[nodiscard]] double median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto mid = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[mid - 1] + values[mid]) / 2.0;
    }
    return values[mid];
}

[[nodiscard]] double sharpe(const std::vector<double>& returns)
{
    if (returns.size() < 2) {
        return 0.0;
    }
    const auto mean = std::accumulate(returns.begin(), returns.end(), 0.0) / static_cast<double>(returns.size());
    double variance = 0.0;
    for (const auto value : returns) {
        variance += (value - mean) * (value - mean);
    }
    variance /= static_cast<double>(returns.size() - 1);
    const auto stddev = std::sqrt(variance);
    return stddev > 0.0 ? (mean / stddev) * std::sqrt(static_cast<double>(returns.size())) : 0.0;
}

[[nodiscard]] double pnl_at_offset(const ValidationRecord& record, std::uint64_t offset)
{
    switch (offset) {
    case 1000:
        return record.pnl_1s;
    case 2000:
        return record.pnl_2s;
    case 5000:
        return record.pnl_5s;
    case 10000:
        return record.pnl_10s;
    case 30000:
        return record.pnl_30s;
    default:
        return record.pnl_30s;
    }
}

} // namespace

RiskAnalyzer::RiskAnalyzer(Config config)
    : config_(std::move(config))
{
}

RiskMetrics RiskAnalyzer::analyze(const std::vector<ValidationRecord>& records) const
{
    RiskMetrics metrics;
    metrics.signals = records.size();
    if (records.empty()) {
        return metrics;
    }

    std::vector<double> pnl_values;
    std::vector<double> hold_times;
    pnl_values.reserve(records.size());
    hold_times.reserve(records.size());

    double gross_profit = 0.0;
    double gross_loss = 0.0;
    double cumulative = 0.0;
    double peak = 0.0;
    std::uint64_t losing_streak = 0;
    std::uint64_t max_losing_streak = 0;
    std::uint64_t successful = 0;
    std::map<std::uint64_t, double> hold_pnl_sum;
    std::map<std::uint64_t, std::uint64_t> hold_pnl_count;

    auto first_ts = records.front().trade.signal_ts;
    auto last_ts = records.front().trade.signal_ts;

    for (const auto& record : records) {
        first_ts = std::min(first_ts, record.trade.signal_ts);
        last_ts = std::max(last_ts, record.trade.signal_ts);
        successful += record.successful_prediction ? 1 : 0;

        const auto pnl = record.pnl_30s;
        pnl_values.push_back(pnl);
        hold_times.push_back(static_cast<double>(record.time_to_peak_ms));

        if (pnl > 0.0) {
            ++metrics.wins;
            gross_profit += pnl;
            losing_streak = 0;
        } else {
            ++metrics.losses;
            gross_loss += std::abs(pnl);
            ++losing_streak;
            max_losing_streak = std::max(max_losing_streak, losing_streak);
        }

        cumulative += pnl;
        peak = std::max(peak, cumulative);
        metrics.drawdown = std::max(metrics.drawdown, peak - cumulative);

        for (const auto offset : {1000ULL, 2000ULL, 5000ULL, 10000ULL, 30000ULL}) {
            hold_pnl_sum[offset] += pnl_at_offset(record, offset);
            ++hold_pnl_count[offset];
        }
    }

    const auto count = static_cast<double>(records.size());
    metrics.prediction_accuracy = static_cast<double>(successful) / count;
    metrics.winrate = static_cast<double>(metrics.wins) / count;
    metrics.average_profit = metrics.wins > 0 ? gross_profit / static_cast<double>(metrics.wins) : 0.0;
    metrics.average_loss = metrics.losses > 0 ? gross_loss / static_cast<double>(metrics.losses) : 0.0;
    metrics.expected_value = std::accumulate(pnl_values.begin(), pnl_values.end(), 0.0) / count;
    metrics.sharpe = sharpe(pnl_values);
    metrics.maximum_losing_streak = max_losing_streak;
    metrics.average_hold_time_ms =
        std::accumulate(hold_times.begin(), hold_times.end(), 0.0) / static_cast<double>(hold_times.size());
    metrics.median_hold_time_ms = median(std::move(hold_times));

    double best_hold_ev = -std::numeric_limits<double>::infinity();
    for (const auto& [offset, sum] : hold_pnl_sum) {
        const auto avg = sum / static_cast<double>(hold_pnl_count[offset]);
        if (avg > best_hold_ev) {
            best_hold_ev = avg;
            metrics.best_hold_time_ms = offset;
        }
    }

    if (last_ts > first_ts) {
        const auto hours = static_cast<double>(last_ts - first_ts) / 3600000.0;
        metrics.signals_per_hour = hours > 0.0 ? count / hours : 0.0;
    }

    return metrics;
}

} // namespace latency_arb
