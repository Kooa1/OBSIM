#include "displaycapturepool.h"
#include "dxgiscreencaptor.h"
#include <algorithm>

DisplayCapturePool& DisplayCapturePool::instance() {
    static DisplayCapturePool pool;
    return pool;
}

DisplayCapturePool::~DisplayCapturePool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, entry] : pool_) {
        if (entry->captor) {
            entry->captor->stop();
        }
    }
    pool_.clear();
}

std::shared_ptr<DisplayCapturePool::PoolEntry>
DisplayCapturePool::get_or_create_entry(const CaptorConfig& config) {
    auto key = std::make_pair(config.offset_x, config.offset_y);
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        return it->second;
    }

    // Create new entry with a real DXGIScreenCaptor
    auto entry = std::make_shared<PoolEntry>();
    entry->config = config;

    auto captor = std::make_unique<DXGIScreenCaptor>();
    captor->apply_config(config);

    // Set up frame distribution callback
    auto entry_weak = std::weak_ptr<PoolEntry>(entry);
    captor->on_frame_captured = [this, entry_weak](AVFramePtr frame) {
        auto entry = entry_weak.lock();
        if (entry) {
            distribute_frame(entry, std::move(frame));
        }
    };

    entry->captor = std::move(captor);
    entry->captor->start();
    pool_[key] = entry;
    return entry;
}

void DisplayCapturePool::distribute_frame(std::shared_ptr<PoolEntry> entry, AVFramePtr frame) {
    // Copy consumer list under short lock, then call callbacks without lock.
    // This prevents callbacks (which may invoke push_frame / notify_frame_ready)
    // from blocking unregister_consumer() while holding the pool mutex.
    std::vector<std::pair<int, std::function<void(AVFramePtr)>>> consumers_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        consumers_copy = entry->consumers;
    }

    for (auto& [id, cb] : consumers_copy) {
        if (cb) {
            cb(frame);
        }
    }
}

int DisplayCapturePool::register_consumer(const CaptorConfig& config,
                                           std::function<void(AVFramePtr)> frame_slot) {
    if (!frame_slot) return -1;

    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = get_or_create_entry(config);
    int id = next_id_++;
    entry->consumers.emplace_back(id, std::move(frame_slot));
    return id;
}

void DisplayCapturePool::unregister_consumer(int consumer_id) {
    if (consumer_id < 0) return;

    std::unique_ptr<DXGIScreenCaptor> captor_to_stop;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = pool_.begin(); it != pool_.end(); ++it) {
            auto& consumers = it->second->consumers;
            auto cit = std::remove_if(consumers.begin(), consumers.end(),
                [consumer_id](const auto& pair) { return pair.first == consumer_id; });
            if (cit != consumers.end()) {
                consumers.erase(cit, consumers.end());
                // If no more consumers, remove entry and take captor out
                if (consumers.empty()) {
                    if (it->second->captor) {
                        captor_to_stop = std::move(it->second->captor);
                        // NOT clearing on_frame_captured here: it would race with the
                        // capture thread reading/calling it. After pool_.erase(it) below,
                        // the lambda's entry_weak.lock() naturally returns null, so any
                        // in-flight on_frame_captured call becomes a safe no-op.
                    }
                    pool_.erase(it);
                }
                break;
            }
        }
    }

    // Stop captor outside the mutex to avoid blocking other pool operations
    // (e.g. AcqNextFrame timeout in captor->stop() → thread.join()).
    if (captor_to_stop) {
        captor_to_stop->stop();
    }
}