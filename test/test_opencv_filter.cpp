#include "../src/core/filter/opencvfilter.h"

#include <iostream>
#include <cassert>
#include <cmath>

static int passed = 0;
static int failed = 0;

#define TEST(name) std::cout << "  TEST: " << name << "... "
#define PASS() do { std::cout << "PASS\n"; passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; failed++; } while(0)

static bool mats_equal(const cv::Mat &a, const cv::Mat &b) {
    if (a.size() != b.size()) return false;
    if (a.type() != b.type()) return false;
    return cv::norm(a, b, cv::NORM_INF) == 0;
}

void test_default_params() {
    TEST("default params are all disabled");
    OpenCVFilter filter;
    assert(!filter.is_active());
    PASS();
}

void test_set_and_get_params() {
    TEST("set_params / params round-trip");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_flip = true;
    p.flip_code = 1;
    p.enable_grayscale = true;
    p.enable_gaussian_blur = true;
    p.blur_ksize = 7;
    filter.set_params(p);

    FilterParams got = filter.params();
    assert(got.enable_flip == true);
    assert(got.flip_code == 1);
    assert(got.enable_grayscale == true);
    assert(got.blur_ksize == 7);
    PASS();
}

void test_is_active() {
    TEST("is_active reflects enabled params");
    OpenCVFilter filter;
    assert(!filter.is_active());

    FilterParams p;
    p.enable_flip = true;
    filter.set_params(p);
    assert(filter.is_active());

    p.enable_flip = false;
    filter.set_params(p);
    assert(!filter.is_active());
    PASS();
}

void test_apply_flip_horizontal() {
    TEST("flip horizontal");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_flip = true;
    p.flip_code = 1;
    filter.set_params(p);

    cv::Mat frame(10, 10, CV_8UC4);
    cv::randu(frame, 0, 255);
    cv::Mat original = frame.clone();
    filter.apply(frame);
    assert(!mats_equal(frame, original));
    PASS();
}

void test_apply_grayscale() {
    TEST("grayscale produces single-channel-equivalent output");
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
            assert(px[0] == px[1] && px[1] == px[2]);
        }
    }
    PASS();
}

void test_apply_gaussian_blur() {
    TEST("gaussian blur changes pixels");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_gaussian_blur = true;
    filter.set_params(p);

    cv::Mat frame(20, 20, CV_8UC4);
    cv::randu(frame, 0, 255);
    cv::Mat original = frame.clone();
    filter.apply(frame);

    double diff = cv::norm(frame, original, cv::NORM_L1);
    assert(diff > 0);
    PASS();
}

void test_apply_sharpen() {
    TEST("sharpen modifies image");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_sharpen = true;
    filter.set_params(p);

    cv::Mat frame(20, 20, CV_8UC4);
    cv::randu(frame, 0, 255);
    cv::Mat original = frame.clone();
    filter.apply(frame);
    assert(!mats_equal(frame, original));
    PASS();
}

void test_apply_edge_detect() {
    TEST("edge detect produces output");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_edge_detect = true;
    filter.set_params(p);

    cv::Mat frame(20, 20, CV_8UC4, cv::Scalar(0, 0, 0, 255));
    cv::rectangle(frame, cv::Rect(5, 5, 10, 10), cv::Scalar(255, 255, 255, 255), cv::FILLED);
    filter.apply(frame);
    assert(!mats_equal(frame, cv::Mat(20, 20, CV_8UC4, cv::Scalar(0, 0, 0, 255))));
    PASS();
}

void test_apply_color_adjust_brightness() {
    TEST("brightness adjustment changes pixels");
    OpenCVFilter filter;
    FilterParams p;
    p.enable_color_adjust = true;
    p.brightness = 0.5f;
    filter.set_params(p);

    cv::Mat frame(10, 10, CV_8UC4, cv::Scalar(100, 100, 100, 255));
    filter.apply(frame);

    cv::Vec4b px = frame.at<cv::Vec4b>(0, 0);
    assert(px[0] > 100);
    PASS();
}

void test_apply_to_avframe() {
    TEST("apply_to_avframe wrapper works");
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
    assert(memcmp(data.data(), original.data(), data.size()) != 0);
    PASS();
}

void test_apply_noop_when_inactive() {
    TEST("apply is no-op when no filter active");
    OpenCVFilter filter;
    cv::Mat frame(10, 10, CV_8UC4, cv::Scalar(50, 100, 150, 255));
    cv::Mat original = frame.clone();
    filter.apply(frame);
    assert(mats_equal(frame, original));
    PASS();
}

int main() {
    std::cout << "OpenCVFilter Unit Tests\n";
    std::cout << "=======================\n\n";

    test_default_params();
    test_set_and_get_params();
    test_is_active();
    test_apply_flip_horizontal();
    test_apply_grayscale();
    test_apply_gaussian_blur();
    test_apply_sharpen();
    test_apply_edge_detect();
    test_apply_color_adjust_brightness();
    test_apply_to_avframe();
    test_apply_noop_when_inactive();

    std::cout << "\n=======================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";

    return failed > 0 ? 1 : 0;
}