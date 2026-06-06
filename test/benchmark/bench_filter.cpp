#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "core/filter/opencvfilter.h"

struct FilterResult {
    const char* label;
    int width;
    int height;
    int iterations;
    double ms;
    double ms_per_frame;
};

static void print_result(const FilterResult& r) {
    std::cout << "  " << std::left << std::setw(55) << r.label
              << std::right << std::setw(8) << r.iterations << " frames in "
              << std::setw(8) << std::fixed << std::setprecision(1) << r.ms
              << " ms  |  " << std::setw(8) << std::fixed << std::setprecision(3)
              << r.ms_per_frame << " ms/frame\n";
}

int main() {
    constexpr int N = 1000;
    constexpr int SIZES[][2] = {{1920, 1080}, {1280, 720}, {854, 480}};

    std::mt19937 rng(42);

    std::cout << "============================================\n";
    std::cout << "  OpenCV Filter Performance Benchmark\n";
    std::cout << "============================================\n\n";

    for (int s = 0; s < 3; ++s) {
        int w = SIZES[s][0], h = SIZES[s][1];
        std::cout << "--- Resolution: " << w << "x" << h << " ---\n";

        // 1. clone-only baseline
        {
            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                (void)copy;
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone only (baseline)", w, h, N, ms, ms / N});
        }

        OpenCVFilter filter;

        // 2. no-op filter (is_active = false)  -- measures overhead of calling apply() when nothing to do
        {
            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                filter.apply(copy);
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone + no-op apply (disabled)", w, h, N, ms, ms / N});
        }

        // 3. flip only
        {
            FilterParams p;
            p.enable_flip = true;
            p.flip_code = 1;
            filter.set_params(p);

            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                filter.apply(copy);
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone + flip (horizontal)", w, h, N, ms, ms / N});
        }

        // 4. grayscale only
        {
            FilterParams p;
            p.enable_grayscale = true;
            filter.set_params(p);

            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                filter.apply(copy);
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone + grayscale", w, h, N, ms, ms / N});
        }

        // 5. color adjust only
        {
            FilterParams p;
            p.enable_color_adjust = true;
            p.brightness = 0.1f;
            p.contrast = 1.2f;
            p.saturation = 1.1f;
            filter.set_params(p);

            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                filter.apply(copy);
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone + color adjust (b/c/s)", w, h, N, ms, ms / N});
        }

        // 6. all filters combined
        {
            FilterParams p;
            p.enable_flip = true;
            p.flip_code = 1;
            p.enable_grayscale = true;
            p.enable_color_adjust = true;
            p.brightness = 0.1f;
            p.contrast = 1.2f;
            p.saturation = 1.1f;
            filter.set_params(p);

            cv::Mat frame(h, w, CV_8UC4);
            cv::randu(frame, 0, 255);
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) {
                cv::Mat copy = frame.clone();
                filter.apply(copy);
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            print_result({"clone + all filters combined", w, h, N, ms, ms / N});
        }

        std::cout << "\n";
    }

    return 0;
}
