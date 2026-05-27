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
        || m_params.enable_gaussian_blur
        || m_params.enable_sharpen
        || m_params.enable_edge_detect
        || m_params.enable_color_adjust
        || m_params.enable_crop;
}

void OpenCVFilter::apply(cv::Mat &frame) {
    if (!is_active()) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_params.enable_flip) apply_flip(frame);
    if (m_params.enable_grayscale) apply_grayscale(frame);
    if (m_params.enable_gaussian_blur) apply_gaussian_blur(frame);
    if (m_params.enable_sharpen) apply_sharpen(frame);
    if (m_params.enable_edge_detect) apply_edge_detect(frame);
    if (m_params.enable_color_adjust) apply_color_adjust(frame);
    if (m_params.enable_crop) apply_crop(frame);
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

void OpenCVFilter::apply_gaussian_blur(cv::Mat &frame) {
    int k = m_params.blur_ksize;
    if (k % 2 == 0) k += 1;
    cv::GaussianBlur(frame, frame, cv::Size(k, k), 0);
}

void OpenCVFilter::apply_sharpen(cv::Mat &frame) {
    cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1);
    cv::filter2D(frame, frame, frame.depth(), kernel);
}

void OpenCVFilter::apply_edge_detect(cv::Mat &frame) {
    cv::Mat gray, edges;
    cv::cvtColor(frame, gray, cv::COLOR_BGRA2GRAY);
    cv::Canny(gray, edges, 50, 150);
    cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGRA);
}

void OpenCVFilter::apply_color_adjust(cv::Mat &frame) {
    if (m_params.brightness != 0.0f || m_params.contrast != 1.0f) {
        frame.convertTo(frame, -1, m_params.contrast, m_params.brightness * 255.0);
    }
    if (m_params.saturation != 1.0f) {
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGRA2BGR);
        cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV);
        for (int i = 0; i < hsv.rows; i++) {
            for (int j = 0; j < hsv.cols; j++) {
                auto &s = hsv.at<cv::Vec3b>(i, j)[1];
                s = cv::saturate_cast<uchar>(s * m_params.saturation);
            }
        }
        cv::cvtColor(hsv, hsv, cv::COLOR_HSV2BGR);
        cv::cvtColor(hsv, frame, cv::COLOR_BGR2BGRA);
    }
}

void OpenCVFilter::apply_crop(cv::Mat &frame) {
    cv::Rect r = m_params.crop_rect;
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (r.width <= 0 || r.height <= 0) return;
    if (r.x + r.width > frame.cols) r.width = frame.cols - r.x;
    if (r.y + r.height > frame.rows) r.height = frame.rows - r.y;
    frame = frame(r).clone();
}