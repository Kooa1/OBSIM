#ifndef OBSIM_OPENCV_FILTER_H
#define OBSIM_OPENCV_FILTER_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <memory>
#include <functional>
#include <mutex>

struct FilterParams {
    bool enable_flip = false;
    int flip_code = 0;

    bool enable_grayscale = false;

    bool enable_gaussian_blur = false;
    int blur_ksize = 5;

    bool enable_sharpen = false;

    bool enable_edge_detect = false;

    bool enable_color_adjust = false;
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;

    bool enable_crop = false;
    cv::Rect crop_rect;
};

class OpenCVFilter {
public:
    OpenCVFilter();
    ~OpenCVFilter() = default;

    void set_params(const FilterParams &params);
    FilterParams params() const;

    void apply(cv::Mat &frame);

    void apply_to_avframe(uint8_t *data, int width, int height, int stride);

    bool is_active() const;

private:
    void apply_flip(cv::Mat &frame);
    void apply_grayscale(cv::Mat &frame);
    void apply_gaussian_blur(cv::Mat &frame);
    void apply_sharpen(cv::Mat &frame);
    void apply_edge_detect(cv::Mat &frame);
    void apply_color_adjust(cv::Mat &frame);
    void apply_crop(cv::Mat &frame);

    mutable std::mutex m_mutex;
    FilterParams m_params;
};

#endif