#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <vector>
#include <utility>
#include "base/videocaptor.h"
#include "utils/ffmpegfactory.h"

class DXGIScreenCaptor;

/// @brief Manages shared DXGI capture sessions per display output.
/// Ensures only one DuplicateOutput call per display; multiple consumers share frames.
class DisplayCapturePool {
public:
    static DisplayCapturePool& instance();

    /// @brief Register a consumer for a given display config.
    /// @param config Display capture config (offset + size identifies the display).
    /// @param frame_slot Callback receiving captured AVFrames (called from capture thread).
    /// @return Consumer ID for unregistration.
    int register_consumer(const CaptorConfig& config,
                          std::function<void(AVFramePtr)> frame_slot);

    /// @brief Unregister a consumer by ID.
    void unregister_consumer(int consumer_id);

private:
    DisplayCapturePool() = default;
    ~DisplayCapturePool();
    DisplayCapturePool(const DisplayCapturePool&) = delete;
    DisplayCapturePool& operator=(const DisplayCapturePool&) = delete;

    struct PoolEntry {
        std::unique_ptr<DXGIScreenCaptor> captor;  // The real DXGI captor (one per display)
        CaptorConfig config;
        std::vector<std::pair<int, std::function<void(AVFramePtr)>>> consumers;
    };

    int next_id_ = 1;
    std::mutex mutex_;
    // Key: (offset_x, offset_y) uniquely identifies a display
    std::map<std::pair<int, int>, std::shared_ptr<PoolEntry>> pool_;

    std::shared_ptr<PoolEntry> get_or_create_entry(const CaptorConfig& config);
    void distribute_frame(std::shared_ptr<PoolEntry> entry, AVFramePtr frame);
};