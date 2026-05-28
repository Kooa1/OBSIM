#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "core/filter/opencvfilter.h"

static bool mats_equal(const cv::Mat &a, const cv::Mat &b) {
    if (a.size() != b.size()) return false;
    if (a.type() != b.type()) return false;
    return cv::norm(a, b, cv::NORM_INF) == 0;
}

TEST(OpenCVFilterTest, test_default_params) {
    OpenCVFilter filter;
    EXPECT_FALSE(filter.is_active());
}

TEST(OpenCVFilterTest, test_set_and_get_params) {
    OpenCVFilter filter;
    FilterParams p;
    p.enable_flip = true;
    p.flip_code = 1;
    p.enable_grayscale = true;
    filter.set_params(p);

    FilterParams got = filter.params();
    EXPECT_TRUE(got.enable_flip);
    EXPECT_EQ(got.flip_code, 1);
    EXPECT_TRUE(got.enable_grayscale);
}

TEST(OpenCVFilterTest, test_is_active) {
    OpenCVFilter filter;
    EXPECT_FALSE(filter.is_active());

    FilterParams p;
    p.enable_flip = true;
    filter.set_params(p);
    EXPECT_TRUE(filter.is_active());

    p.enable_flip = false;
    filter.set_params(p);
    EXPECT_FALSE(filter.is_active());
}

TEST(OpenCVFilterTest, test_apply_flip_horizontal) {
    OpenCVFilter filter;
    FilterParams p;
    p.enable_flip = true;
    p.flip_code = 1;
    filter.set_params(p);

    cv::Mat frame(10, 10, CV_8UC4);
    cv::randu(frame, 0, 255);
    cv::Mat original = frame.clone();
    filter.apply(frame);
    EXPECT_FALSE(mats_equal(frame, original));
}

TEST(OpenCVFilterTest, test_apply_grayscale) {
    OpenCVFilter filter;
    FilterParams p;
    p.enable_grayscale = true;
    filter.set_params(p);

    cv::Mat frame(10, 10, CV_8UC4, cv::Scalar(100, 150, 200, 255));
    filter.apply(frame);

    cv::Mat expected_gray;
    cv::cvtColor(frame, expected_gray, cv::COLOR_BGRA2GRAY);
    for (int i = 0; i < frame.rows; i++) {
        for (int j = 0; j < frame.cols; j++) {
            cv::Vec4b px = frame.at<cv::Vec4b>(i, j);
            EXPECT_EQ(px[0], px[1]);
            EXPECT_EQ(px[1], px[2]);
        }
    }
}

TEST(OpenCVFilterTest, test_apply_color_adjust_brightness) {
    OpenCVFilter filter;
    FilterParams p;
    p.enable_color_adjust = true;
    p.brightness = 0.5f;
    filter.set_params(p);

    cv::Mat frame(10, 10, CV_8UC4, cv::Scalar(100, 100, 100, 255));
    filter.apply(frame);

    cv::Vec4b px = frame.at<cv::Vec4b>(0, 0);
    EXPECT_GT(px[0], 100);
}

TEST(OpenCVFilterTest, test_apply_to_avframe) {
    OpenCVFilter filter;
    FilterParams p;
    p.enable_flip = true;
    p.flip_code = 1;
    filter.set_params(p);

    int w = 10, h = 10, stride = w * 4;
    std::vector<uint8_t> data(h * stride);
    cv::Mat frame(h, w, CV_8UC4, data.data(), stride);
    cv::randu(frame, 0, 255);
    std::vector<uint8_t> original = data;

    filter.apply_to_avframe(data.data(), w, h, stride);
    EXPECT_NE(memcmp(data.data(), original.data(), data.size()), 0);
}

TEST(OpenCVFilterTest, test_apply_noop_when_inactive) {
    OpenCVFilter filter;
    cv::Mat frame(10, 10, CV_8UC4, cv::Scalar(50, 100, 150, 255));
    cv::Mat original = frame.clone();
    filter.apply(frame);
    EXPECT_TRUE(mats_equal(frame, original));
}