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
    float intensity = m_params.sharpen_intensity;
    float c = 4.0f * intensity;
    float ne = -1.0f * intensity;
    cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
        ne, ne, ne,
        ne, c + 1.0f, ne,
        ne, ne, ne);
    cv::filter2D(frame, frame, frame.depth(), kernel);
}

void OpenCVFilter::apply_edge_detect(cv::Mat &frame) {
    cv::Mat gray, edges;
    cv::cvtColor(frame, gray, cv::COLOR_BGRA2GRAY);
    cv::Canny(gray, edges, m_params.edge_low, m_params.edge_high);
    cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGRA);
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

void OpenCVFilter::apply_crop(cv::Mat &frame) {
    cv::Rect r = m_params.crop_rect;
    if (r.width <= 0 || r.height <= 0) return;
    r &= cv::Rect(0, 0, frame.cols, frame.rows);
    if (r.area() <= 0) {
        frame = cv::Scalar(0, 0, 0, 255);
        return;
    }

    cv::Mat bg = cv::Mat::zeros(frame.size(), frame.type());
    bg = cv::Scalar(0, 0, 0, 255);
    frame(r).copyTo(bg(r));
    bg.copyTo(frame);
}