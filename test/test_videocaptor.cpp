#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/base/videocaptor.h"

class MockVideoCaptor : public VideoCaptor {
public:
    const char* get_input_format_name() const override { return "mock_format"; }
    const char* get_device_name() const override { return "mock_device"; }
    void setup_options(AVDictionary**) override {}

    void public_apply_config(const CaptorConfig &c) { apply_config(c); }
    void public_push_frame(AVFramePtr f) { push_frame(std::move(f)); }
    void public_notify_frame_ready() { notify_frame_ready(); }

    bool is_paused() const { return is_paused_.load(); }
    CaptorConfig config() const { return m_config; }
};

TEST(VideoCaptorTest, InitiallyNotRunning) {
    MockVideoCaptor captor;
    EXPECT_FALSE(captor.is_running());
}

TEST(VideoCaptorTest, StartMakesRunning) {
    MockVideoCaptor captor;
    captor.start();
    EXPECT_TRUE(captor.is_running());
    captor.stop();
}

TEST(VideoCaptorTest, StopMakesNotRunning) {
    MockVideoCaptor captor;
    captor.start();
    captor.stop();
    EXPECT_FALSE(captor.is_running());
}

TEST(VideoCaptorTest, StartStopMultipleTimes) {
    MockVideoCaptor captor;
    captor.start();
    captor.stop();
    captor.start();
    EXPECT_TRUE(captor.is_running());
    captor.stop();
    EXPECT_FALSE(captor.is_running());
}

TEST(VideoCaptorTest, DoubleStartIsSafe) {
    MockVideoCaptor captor;
    captor.start();
    captor.start();
    EXPECT_TRUE(captor.is_running());
    captor.stop();
}

TEST(VideoCaptorTest, PauseSetsFlag) {
    MockVideoCaptor captor;
    EXPECT_FALSE(captor.is_paused());
    captor.pause();
    EXPECT_TRUE(captor.is_paused());
}

TEST(VideoCaptorTest, ResumeClearsFlag) {
    MockVideoCaptor captor;
    captor.pause();
    EXPECT_TRUE(captor.is_paused());
    captor.resume();
    EXPECT_FALSE(captor.is_paused());
}

TEST(VideoCaptorTest, PauseResumeMultipleTimes) {
    MockVideoCaptor captor;
    captor.pause();
    captor.resume();
    captor.pause();
    EXPECT_TRUE(captor.is_paused());
    captor.resume();
    EXPECT_FALSE(captor.is_paused());
}

TEST(VideoCaptorTest, ApplyConfigStoresConfig) {
    MockVideoCaptor captor;
    CaptorConfig cfg{10, 20, 1920, 1080};
    captor.public_apply_config(cfg);
    CaptorConfig stored = captor.config();
    EXPECT_EQ(stored.offset_x, 10);
    EXPECT_EQ(stored.offset_y, 20);
    EXPECT_EQ(stored.width, 1920);
    EXPECT_EQ(stored.height, 1080);
}

TEST(VideoCaptorTest, TryPopFrameReturnsNulloptWhenNoQueue) {
    MockVideoCaptor captor;
    auto result = captor.try_pop_frame();
    EXPECT_FALSE(result.has_value());
}

TEST(VideoCaptorTest, PushFrameAndTryPopFrame) {
    MockVideoCaptor captor;
    captor.start();
    AVFramePtr frame(av_frame_alloc(), AVFrameDeleter());
    ASSERT_TRUE(frame != nullptr);
    captor.public_push_frame(std::move(frame));
    auto result = captor.try_pop_frame();
    EXPECT_TRUE(result.has_value());
    captor.stop();
}

TEST(VideoCaptorTest, PushMultipleFramesAndPopLast) {
    MockVideoCaptor captor;
    captor.start();
    AVFramePtr frame1(av_frame_alloc(), AVFrameDeleter());
    AVFramePtr frame2(av_frame_alloc(), AVFrameDeleter());
    AVFramePtr frame3(av_frame_alloc(), AVFrameDeleter());
    ASSERT_TRUE(frame1 && frame2 && frame3);
    auto* ptr3 = frame3.get();
    captor.public_push_frame(std::move(frame1));
    captor.public_push_frame(std::move(frame2));
    captor.public_push_frame(std::move(frame3));
    auto result = captor.try_pop_frame();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().get(), ptr3);
    captor.stop();
}

TEST(VideoCaptorTest, FrameReadyCallbackIsInvoked) {
    MockVideoCaptor captor;
    int call_count = 0;
    captor.set_frame_ready_callback([&call_count]() { ++call_count; });
    captor.public_notify_frame_ready();
    EXPECT_EQ(call_count, 1);
    captor.public_notify_frame_ready();
    EXPECT_EQ(call_count, 2);
}

TEST(VideoCaptorTest, ReplaceFrameReadyCallback) {
    MockVideoCaptor captor;
    int old_count = 0;
    int new_count = 0;
    captor.set_frame_ready_callback([&old_count]() { ++old_count; });
    captor.set_frame_ready_callback([&new_count]() { ++new_count; });
    captor.public_notify_frame_ready();
    EXPECT_EQ(old_count, 0);
    EXPECT_EQ(new_count, 1);
}

TEST(VideoCaptorTest, PauseClearsQueue) {
    MockVideoCaptor captor;
    captor.start();
    AVFramePtr frame(av_frame_alloc(), AVFrameDeleter());
    ASSERT_TRUE(frame != nullptr);
    captor.public_push_frame(std::move(frame));
    captor.pause();
    auto result = captor.try_pop_frame();
    EXPECT_FALSE(result.has_value());
    captor.stop();
}