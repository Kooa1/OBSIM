#ifndef ZPLAYER_DAtASAfeQUEUE_H
#define ZPLAYER_DAtASAfeQUEUE_H

#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <optional>
#include <iostream>
#include <condition_variable>
#include <ranges>

#include "ffmpegfactory.h"

using std::cout;

template<typename T>
class DataSafeQueue {
private:
    const size_t max_size;
    const size_t size_warning_line;

    mutable std::mutex q_mutex;
    std::queue<T> data_queue;

    std::condition_variable cv_not_full;
    std::condition_variable cv_not_empty;

    std::atomic<bool> pause_request;
    std::atomic<bool> stop_request;

public:
    explicit DataSafeQueue(const size_t max_size = 256)
        : max_size(max_size), size_warning_line(max_size / 2), pause_request(false), stop_request(false) {
    }

    ~DataSafeQueue() = default;

    DataSafeQueue(const DataSafeQueue &other) = delete;

    DataSafeQueue operator=(const DataSafeQueue &other) = delete;

    bool push(T value) {
        std::unique_lock<std::mutex> lock(q_mutex);
        if (data_queue.size() >= max_size) {
            cv_not_full.wait(lock, [this]() {
                return data_queue.size() <= size_warning_line ||
                       pause_request.load(std::memory_order_acquire) ||
                       stop_request.load(std::memory_order_acquire);
            });

            if (pause_request.load(std::memory_order_acquire) || stop_request.load(std::memory_order_acquire)) {
                return false;
            }
        }

        data_queue.push(std::move(value));
        cv_not_empty.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(q_mutex);
        if (stop_request.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        if (data_queue.empty()) {
            cv_not_empty.wait(lock, [this]() {
                return !data_queue.empty() ||
                       pause_request.load(std::memory_order_acquire) ||
                       stop_request.load(std::memory_order_acquire);
            });
            if (stop_request.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
        }

        const auto value = std::move(data_queue.front());
        data_queue.pop();
        cv_not_full.notify_one();
        return std::move(value);
    }

    bool try_push(T value) {
        std::unique_lock<std::mutex> lock(q_mutex);
        if (data_queue.size() >= max_size) {
            cv_not_empty.notify_one();
            std::this_thread::sleep_for(std::chrono::microseconds(10000));

            if (data_queue.size() >= max_size) {
                return false;
            }
        }
        data_queue.push(std::move(value));
        cv_not_empty.notify_one();
        return true;
    }

    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lock(q_mutex);
        if (stop_request.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        if (data_queue.empty()) {
            // cv_not_empty.wait(lock, [this]() {
            //     return !data_queue.empty() ||
            //            pause_request.load(std::memory_order_acquire) ||
            //            stop_request.load(std::memory_order_acquire);
            // });
            cv_not_empty.wait_for(lock, std::chrono::seconds(1), [this]() {
                return !data_queue.empty() ||
                       pause_request.load(std::memory_order_acquire) ||
                       stop_request.load(std::memory_order_acquire);;
            });

            if (data_queue.empty()) {
                return std::nullopt;
            }

            if (stop_request.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
        }
        auto value = std::move(data_queue.front());
        data_queue.pop();

        return value;
    }

    inline void push_poison_pill() {
        std::unique_lock<std::mutex> lock(q_mutex);
        data_queue.push(std::move(T()));
        cv_not_empty.notify_one();
    }

    size_t get_queue_size() const {
        std::lock_guard<std::mutex> lock_guard(q_mutex);
        return data_queue.size();
    }

    inline size_t get_queue_max_size() const {
        std::lock_guard<std::mutex> lock_guard(q_mutex);
        return max_size;
    }

    inline T peek() const {
        std::lock_guard<std::mutex> lock_guard(q_mutex);
        return data_queue.front();
    }

    inline bool is_empty() {
        std::lock_guard<std::mutex> lock_guard(q_mutex);
        return data_queue.empty();
    }

    inline std::atomic<bool> get_pause_state() const {
        return pause_request.load(std::memory_order_acquire);
    }

    inline std::atomic<bool> get_stop_state() const {
        return stop_request.load(std::memory_order_acquire);
    }

    inline void nut_full_wake_up() {
        cv_not_full.notify_one();
    }

    inline void nut_empty_wake_up() {
        cv_not_empty.notify_one();
    }

    inline void set_pause_state(const bool state) {
        pause_request.store(state, std::memory_order_release);
    }

    inline void set_stop_state(const bool state) {
        stop_request.store(state, std::memory_order_release);
    }

    inline void stop_work() {
        stop_request.store(true, std::memory_order_release);
        cv_not_empty.notify_one();
        cv_not_full.notify_one();
    }

    void clean_queue() {
        std::unique_lock<std::mutex> lock(q_mutex);
        pause_request.store(false, std::memory_order_release);
        stop_request.store(false, std::memory_order_release);
        while (!data_queue.empty()) {
            data_queue.pop();
        }
    }
};

#endif //ZPLAYER_DAtASAfeQUEUE_H
