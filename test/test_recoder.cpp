#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/base/recoder.h"
#include "core/fileoutput.h"

TEST(RecoderTest, initially_not_recording)
{
    Recoder recoder;
    EXPECT_FALSE(recoder.is_recording());
}

TEST(RecoderTest, start_sets_recording_state)
{
    Recoder recoder;
    recoder.start("test.mp4", 1920, 1080, 30);
    EXPECT_TRUE(recoder.is_recording());
    recoder.stop();
}

TEST(RecoderTest, stop_clears_recording_state)
{
    Recoder recoder;
    recoder.start("test.mp4", 1920, 1080, 30);
    recoder.stop();
    EXPECT_FALSE(recoder.is_recording());
}

TEST(RecoderTest, feed_frame_nullptr_does_not_crash)
{
    Recoder recoder;
    EXPECT_NO_THROW(recoder.feed_frame(nullptr, 0, 0, 0));
}

TEST(RecoderTest, feed_frame_during_recording_no_crash)
{
    Recoder recoder;
    recoder.start("test.mp4", 1920, 1080, 30);
    EXPECT_NO_THROW(recoder.feed_frame(nullptr, 0, 0, 0));
    recoder.stop();
}

TEST(RecoderTest, volume_control)
{
    Recoder recoder;

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
    Recoder recoder;

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

TEST(RecoderTest, output_channel_management)
{
    Recoder recoder;
    EXPECT_EQ(recoder.output_count(), 0u);

    FileOutput output;
    recoder.register_output(&output);
    EXPECT_EQ(recoder.output_count(), 1u);

    recoder.unregister_output(&output);
    EXPECT_EQ(recoder.output_count(), 0u);
}

TEST(RecoderTest, duplicate_register_no_duplicate)
{
    Recoder recoder;
    FileOutput output;
    recoder.register_output(&output);
    recoder.register_output(&output);
    EXPECT_EQ(recoder.output_count(), 1u);
}
