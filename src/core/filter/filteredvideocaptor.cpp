#include "filteredvideocaptor.h"

#ifdef _WIN32
#include <windows.h>
#endif

FilteredVideoCaptor::FilteredVideoCaptor()
    : m_filter(std::make_unique<OpenCVFilter>())
{
}

FilteredVideoCaptor::~FilteredVideoCaptor() {
    stop();
}

void FilteredVideoCaptor::set_filter_enabled(bool enabled) {
    m_filter_enabled.store(enabled);
}

bool FilteredVideoCaptor::is_filter_enabled() const {
    return m_filter_enabled.load();
}

std::optional<AVFramePtr> FilteredVideoCaptor::try_pop_filtered_frame() {
    if (!m_filtered_queue) return std::nullopt;
    return m_filtered_queue->try_pop_drain();
}

void FilteredVideoCaptor::start() {
    VideoCaptor::start();
}

void FilteredVideoCaptor::stop() {
    stop_filter();
    VideoCaptor::stop();
}

void FilteredVideoCaptor::pause() {
    VideoCaptor::pause();
}

void FilteredVideoCaptor::resume() {
    VideoCaptor::resume();
}

void FilteredVideoCaptor::start_filter() {
    if (m_filter_running.load()) return;
    m_filtered_queue = std::make_unique<DataSafeQueue<AVFramePtr>>(64);
    m_filter_enabled.store(true);
    m_filter_running.store(true);
    m_filter_thread = std::thread([this]() { filter_loop(); });
#ifdef _WIN32
    SetThreadPriority(m_filter_thread.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void FilteredVideoCaptor::stop_filter() {
    m_filter_running.store(false);
    if (m_filter_thread.joinable()) {
        m_filter_thread.join();
    }
    if (m_filtered_queue) {
        m_filtered_queue->clean_queue();
    }
}

void FilteredVideoCaptor::filter_loop() {
    while (m_filter_running.load()) {
        auto frame = queue->try_pop_drain();
        if (!frame.has_value() || !frame.value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        AVFrame *avf = frame.value().get();
        if (!avf->data[0] || avf->width <= 0 || avf->height <= 0) {
            continue;
        }

        if (m_filter_enabled.load()) {
            int stride = avf->linesize[0] > 0 ? avf->linesize[0] : avf->width * 4;
            cv::Mat mat(avf->height, avf->width, CV_8UC4,
                       avf->data[0], stride);
            m_filter->apply(mat);
        }

        m_filtered_queue->push_no_wait(std::move(frame.value()));
    }
}