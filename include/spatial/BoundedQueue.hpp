#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace spatial {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFullCv_.wait(lock, [this]() { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(item));
        notEmptyCv_.notify_one();
        return true;
    }

    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmptyCv_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        notFullCv_.notify_one();
        return true;
    }

    bool tryPop(T& out) {
        std::scoped_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        notFullCv_.notify_one();
        return true;
    }

    void close() {
        std::scoped_lock<std::mutex> lock(mutex_);
        closed_ = true;
        notFullCv_.notify_all();
        notEmptyCv_.notify_all();
    }

    std::size_t size() const {
        std::scoped_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    const std::size_t capacity_;

    mutable std::mutex mutex_;
    std::condition_variable notEmptyCv_;
    std::condition_variable notFullCv_;
    std::deque<T> queue_;
    bool closed_ = false;
};

} // namespace spatial
