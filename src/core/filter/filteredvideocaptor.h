#ifndef OBSIM_FILTERED_VIDEO_CAPTOR_H
#define OBSIM_FILTERED_VIDEO_CAPTOR_H

#include "../base/videocaptor.h"
#include "opencvfilter.h"

/// @brief Decorator for VideoCaptor that applies OpenCV filters to captured frames
class FilteredVideoCaptor : public VideoCaptor {
public:
    /// @brief Constructor wrapping an inner video captor
    /// @param inner Unique pointer to the underlying video captor
    explicit FilteredVideoCaptor(std::unique_ptr<VideoCaptor> inner);
    ~FilteredVideoCaptor() override;

    /// @brief Enable or disable filter processing
    /// @param enabled Whether to apply filters
    void set_filter_enabled(bool enabled);
    /// @brief Check if filter processing is enabled
    /// @return True if filter is enabled
    bool is_filter_enabled() const;
    /// @brief Get pointer to OpenCV filter
    /// @return Pointer to OpenCVFilter instance
    OpenCVFilter* filter() { return m_filter.get(); }
    /// @brief Try to pop a filtered frame from the filter output queue
    /// @return Optional AVFramePtr
    std::optional<AVFramePtr> try_pop_filtered_frame();

    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;

    /// @brief Start filter processing thread
    void start_filter();
    /// @brief Stop filter processing thread and drain queues
    void stop_filter();
    bool is_filter_running() const { return m_filter_running.load(); }

protected:
    const char* get_input_format_name() const override { return m_inner->get_input_format_name(); }
    const char* get_device_name() const override { return m_inner->get_device_name(); }
    void setup_options(AVDictionary** opts) override { m_inner->setup_options(opts); }

    /// @brief Override frame push to route through raw queue when filter is active
    void push_frame(AVFramePtr frame) override;
    /// @brief Filter processing loop running on a separate thread
    void filter_loop();

    std::unique_ptr<VideoCaptor> m_inner;               ///< Underlying video captor
    std::unique_ptr<OpenCVFilter> m_filter;             ///< OpenCV filter instance
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_raw_queue;       ///< Queue for raw frames before filtering
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_filtered_queue;  ///< Queue for filtered output frames
    std::thread m_filter_thread;                        ///< Filter processing thread
    std::atomic<bool> m_filter_running{false};          ///< Whether filter thread is running
    std::atomic<bool> m_filter_enabled{false};          ///< Whether filter processing is enabled
};

#endif