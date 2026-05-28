#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/base/recoder.h"

class MockRecoder : public Recoder {
public:
    AVFormatOutputContextPtr create_format_context() override
    {
        return nullptr;
    }
    bool open_io(AVFormatOutputContextPtr &fmt_ctx) override
    {
        return false;
    }
};

TEST(RecoderTest, initially_not_recording)
{
    MockRecoder recoder;
    EXPECT_FALSE(recoder.is_recording());
}

TEST(RecoderTest, start_sets_recording_state)
{
    MockRecoder recoder;
    recoder.start("test.mp4", 1920, 1080, 30);
    EXPECT_TRUE(recoder.is_recording());
    recoder.stop();
}

TEST(RecoderTest, stop_clears_recording_state)
{
    MockRecoder recoder;
    recoder.start("test.mp4", 1920, 1080, 30);
    recoder.stop();
    EXPECT_FALSE(recoder.is_recording());
}

TEST(RecoderTest, feed_frame_nullptr_does_not_crash)
{
    MockRecoder recoder;
    EXPECT_NO_THROW(recoder.feed_frame(nullptr, 0, 0, 0));
}

TEST(RecoderTest, volume_control)
{
    MockRecoder recoder;

    EXPECT_FLOAT_EQ(recoder.get_system_volume(), 0.7f);
    EXPECT_FLOAT_EQ(recoder.get_mic_volume(), 0.7f);

    recoder.set_system_volume(0.5f);
    EXPECT_FLOAT_EQ(recoder.get_system_volume(), 0.5f);

    recoder.set_system_volume(0.0f);
    EXPECT_FLOAT_EQ(recoder.get_system_volume(), 0.0f);

    recoder.set_system_volume(1.0f);
    EXPECT_FLOAT_EQ(recoder.get_system_volume(), 1.0f);

    recoder.set_mic_volume(0.3f);
    EXPECT_FLOAT_EQ(recoder.get_mic_volume(), 0.3f);

    recoder.set_mic_volume(0.0f);
    EXPECT_FLOAT_EQ(recoder.get_mic_volume(), 0.0f);

    recoder.set_mic_volume(1.0f);
    EXPECT_FLOAT_EQ(recoder.get_mic_volume(), 1.0f);
}

TEST(RecoderTest, mute_control)
{
    MockRecoder recoder;

    EXPECT_FALSE(recoder.is_system_muted());
    EXPECT_FALSE(recoder.is_mic_muted());

    recoder.set_system_muted(true);
    EXPECT_TRUE(recoder.is_system_muted());

    recoder.set_system_muted(false);
    EXPECT_FALSE(recoder.is_system_muted());

    recoder.set_mic_muted(true);
    EXPECT_TRUE(recoder.is_mic_muted());

    recoder.set_mic_muted(false);
    EXPECT_FALSE(recoder.is_mic_muted());
}