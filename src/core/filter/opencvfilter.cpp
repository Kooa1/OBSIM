#include "opencvfilter.h"

OpenCVFilter::OpenCVFilter()
    : m_params() {
}

void OpenCVFilter::set_params(const FilterParams &params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
}

FilterParams OpenCVFilter::params() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_params;
}

bool OpenCVFilter::is_active() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_params.enable_flip
        || m_params.enable_grayscale
        || m_params.enable_color_adjust;
}

void OpenCVFilter::apply(cv::Mat &frame) {
    if (!is_active()) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_params.enable_flip) apply_flip(frame);
    if (m_params.enable_grayscale) apply_grayscale(frame);
    if (m_params.enable_color_adjust) apply_color_adjust(frame);
}

void OpenCVFilter::apply_to_avframe(uint8_t *data, int width, int height, int stride) {
    cv::Mat mat(height, width, CV_8UC4, data, stride);
    apply(mat);
}

void OpenCVFilter::apply_flip(cv::Mat &frame) {
    cv::flip(frame, frame, m_params.flip_code);
}

void OpenCVFilter::apply_grayscale(cv::Mat &frame) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGRA2GRAY);
    cv::cvtColor(gray, frame, cv::COLOR_GRAY2BGRA);
}


void OpenCVFilter::apply_color_adjust(cv::Mat &frame) {
    std::vector<cv::Mat> ch(4);
    cv::split(frame, ch);

    cv::Mat bgr;
    cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, bgr);

    if (m_params.brightness != 0.0f || m_params.contrast != 1.0f) {
        bgr.convertTo(bgr, -1, m_params.contrast, m_params.brightness * 255.0);
    }
    if (m_params.saturation != 1.0f) {
        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> hsv_ch(3);
        cv::split(hsv, hsv_ch);
        hsv_ch[1] = hsv_ch[1] * m_params.saturation;
        cv::merge(hsv_ch, hsv);
        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    }

    std::vector<cv::Mat> bgr_ch(3);
    cv::split(bgr, bgr_ch);
    cv::merge(std::vector<cv::Mat>{bgr_ch[0], bgr_ch[1], bgr_ch[2], ch[3]}, frame);
}

