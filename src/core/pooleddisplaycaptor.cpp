#include "pooleddisplaycaptor.h"
#include "displaycapturepool.h"

PooledDisplayCaptor::PooledDisplayCaptor(const CaptorConfig& config) {
    m_config = config;
}

PooledDisplayCaptor::~PooledDisplayCaptor() {
    stop();
}

void PooledDisplayCaptor::start() {
    if (is_capturing.load()) return;

    queue = std::make_unique<DataSafeQueue<AVFramePtr>>(64);
    is_capturing.store(true);

    // Register with the DisplayCapturePool.
    // The callback is called from the DXGI capture thread via distribute_frame.
    consumer_id_ = DisplayCapturePool::instance().register_consumer(m_config,
        [this](AVFramePtr frame) {
            if (!is_capturing.load()) return;
            push_frame(std::move(frame));
            notify_frame_ready();
        });

    if (consumer_id_ < 0) {
        is_capturing.store(false);
    }
}

void PooledDisplayCaptor::stop() {
    is_capturing.store(false);

    if (consumer_id_ >= 0) {
        DisplayCapturePool::instance().unregister_consumer(consumer_id_);
        consumer_id_ = -1;
    }

    // Clear the frame_ready_callback (set by FilteredVideoCaptor::start())
    // to prevent any dangling reference to FilteredVideoCaptor's `this`.
    set_frame_ready_callback(nullptr);

    if (queue) {
        queue->clean_queue();
    }
}

void PooledDisplayCaptor::pause() {
    VideoCaptor::pause();
}

void PooledDisplayCaptor::resume() {
    VideoCaptor::resume();
}