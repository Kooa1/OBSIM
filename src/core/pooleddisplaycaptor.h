#ifndef OBSIM_POOLED_DISPLAY_CAPTOR_H
#define OBSIM_POOLED_DISPLAY_CAPTOR_H

#include "base/videocaptor.h"

/// @brief VideoCaptor that shares a DXGI capture session via DisplayCapturePool.
/// Multiple PooledDisplayCaptors targeting the same display share
/// a single underlying DXGIScreenCaptor managed by the pool.
class PooledDisplayCaptor : public VideoCaptor {
public:
    explicit PooledDisplayCaptor(const CaptorConfig& config);
    ~PooledDisplayCaptor() override;

    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;

protected:
    const char* get_input_format_name() const override { return "pooled_dxgi"; }
    const char* get_device_name() const override { return "desktop"; }

private:
    int consumer_id_ = -1;  ///< Consumer ID returned by DisplayCapturePool::register_consumer
};

#endif // OBSIM_POOLED_DISPLAY_CAPTOR_H