#ifndef OBSIM_OPENCV_FILTER_H
#define OBSIM_OPENCV_FILTER_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <memory>
#include <functional>
#include <mutex>

/// @brief Parameters for OpenCV image processing filters
struct FilterParams {
    bool enable_flip = false;          ///< Whether to flip the image
    int flip_code = 0;                 ///< Flip direction code (0=vertical, >0=horizontal, <0=both)

    bool enable_grayscale = false;     ///< Whether to convert to grayscale

    bool enable_color_adjust = false;  ///< Whether to apply color adjustments
    float brightness = 0.0f;           ///< Brightness adjustment (-1.0 to 1.0)
    float contrast = 1.0f;             ///< Contrast multiplier
    float saturation = 1.0f;           ///< Saturation multiplier
};

/// @brief OpenCV-based image filter that applies flip, grayscale, and color adjustments
class OpenCVFilter {
public:
    OpenCVFilter();
    ~OpenCVFilter() = default;

    /// @brief Set filter parameters in a thread-safe manner
    /// @param params New filter parameters
    void set_params(const FilterParams &params);
    FilterParams params() const;

    /// @brief Apply all enabled filters to the frame in-place
    /// @param frame OpenCV Mat to process
    void apply(cv::Mat &frame);

    /// @brief Apply filters to an AVFrame's raw data
    /// @param data Raw pixel data pointer
    /// @param width Frame width
    /// @param height Frame height
    /// @param stride Row stride in bytes
    void apply_to_avframe(uint8_t *data, int width, int height, int stride);

    /// @brief Check if any filter is currently active
    /// @return True if at least one filter is enabled
    bool is_active() const;

private:
    /// @brief Apply horizontal/vertical flip
    void apply_flip(cv::Mat &frame);
    /// @brief Apply grayscale conversion
    void apply_grayscale(cv::Mat &frame);
    /// @brief Apply brightness, contrast, and saturation adjustment
    void apply_color_adjust(cv::Mat &frame);

    mutable std::mutex m_mutex;   ///< Mutex for thread-safe parameter access
    FilterParams m_params;        ///< Current filter parameters
};

#endif