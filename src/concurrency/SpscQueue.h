#pragma once

#include <boost/lockfree/spsc_queue.hpp>

#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>

template <typename T> class SpscQueue {
public:
  explicit SpscQueue(size_t capacity) : queue_(capacity) {}

  SpscQueue(const SpscQueue &) = delete;
  SpscQueue &operator=(const SpscQueue &) = delete;
  size_t size() { return queue_.read_available(); }
  bool push_blocking(const T &item) {
    while (!closed_.load(std::memory_order_acquire)) {
      if (queue_.push(item)) {
        return true;
      }

      std::this_thread::yield();
    }

    return false;
  }

  bool try_push(const T &item) {
    if (closed_.load(std::memory_order_acquire)) {
      return false;
    }

    return queue_.push(item);
  }

  std::optional<T> try_pop() {
    T item;

    if (queue_.pop(item)) {
      return item;
    }

    return std::nullopt;
  }

  void close() { closed_.store(true, std::memory_order_release); }

  bool closed() const { return closed_.load(std::memory_order_acquire); }

  bool consumer_empty() { return queue_.empty(); }

private:
  boost::lockfree::spsc_queue<T> queue_;
  std::atomic<bool> closed_{false};
};