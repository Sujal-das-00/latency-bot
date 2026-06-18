#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>

namespace latency_arb {

template <typename T>
class ThreadSafeQueue {
public:
    void push(T value)
    {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    [[nodiscard]] std::optional<T> try_pop()
    {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    [[nodiscard]] std::optional<T> wait_pop(std::stop_token stop_token)
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, stop_token, [this] { return !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void notify_all()
    {
        cv_.notify_all();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::queue<T> queue_;
};

} // namespace latency_arb
