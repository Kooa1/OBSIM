#ifndef OBSIM_FILTERED_VIDEO_CAPTOR_H
#define OBSIM_FILTERED_VIDEO_CAPTOR_H

#include "../base/videocaptor.h"
#include "opencvfilter.h"

class FilteredVideoCaptor : public VideoCaptor {
public:
    FilteredVideoCaptor();
    ~FilteredVideoCaptor() override;

    void set_filter_enabled(bool enabled);
    bool is_filter_enabled() const;
    OpenCVFilter* filter() { return m_filter.get(); }

    std::optional<AVFramePtr> try_pop_filtered_frame();

    void start();
    void stop();
    void pause();
    void resume();

    void start_filter();
    void stop_filter();
    bool is_filter_running() const { return m_filter_running.load(); }

protected:
    void push_frame(AVFramePtr frame) override;
    void filter_loop();

    std::unique_ptr<OpenCVFilter> m_filter;
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_raw_queue;
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_filtered_queue;
    std::thread m_filter_thread;
    std::atomic<bool> m_filter_running{false};
    std::atomic<bool> m_filter_enabled{false};
};

#endif