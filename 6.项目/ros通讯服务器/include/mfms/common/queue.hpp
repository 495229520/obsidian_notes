#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mfms {

// Thread-safe multi-producer multi-consumer queue
template<typename T>
class MpmcQueue {
public:
    explicit MpmcQueue(std::size_t max_size = 0) : max_size_(max_size) {}

    // Push item, returns false if queue is full (when max_size > 0)
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(std::move(item));
        cv_.notify_one();
        return true;
    }

    // Try to pop item without blocking
    std::optional<T> tryPop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Pop with blocking wait
    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (stopped_ && queue_.empty()) {
            throw std::runtime_error("Queue stopped");
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Drain all items
    std::vector<T> drainAll() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::vector<T> result;
        result.reserve(queue_.size());
        while (!queue_.empty()) {
            result.push_back(std::move(queue_.front()));
            queue_.pop();
        }
        return result;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    std::size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    std::size_t max_size_;
    bool stopped_{false};
};

// Single-producer single-consumer lock-free queue (simple ring buffer)
template<typename T, std::size_t Capacity>
class SpscQueue {
public:
    bool push(const T& item) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        buffer_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> tryPop() {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt; // Empty
        }
        T item = std::move(buffer_[tail]);
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return item;
    }

    bool empty() const {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    bool wasEmpty() const {
        // Check if queue was empty before last push (for wakeup coalescing)
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return ((head + Capacity - 1) % Capacity) == tail;
    }

private:
    std::array<T, Capacity> buffer_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace mfms
