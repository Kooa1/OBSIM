#include <gtest/gtest.h>

#include <QApplication>
#include <memory>

#include "core/audiomanager.h"
#include "core/systemaudiocaptor.h"
#include "core/micaudiocaptor.h"

class MockSystemAudioCaptor : public SystemAudioCaptor {
public:
    MockSystemAudioCaptor() = default;
    ~MockSystemAudioCaptor() override = default;

protected:
    void init_ctx() override {}
    void capture_loop() override {}
    const char* get_input_format_name() const override { return "mock_wasapi"; }
    const char* get_device_name() const override { return ""; }
    void setup_options(AVDictionary**) override {}
};

class MockMicAudioCaptor : public MicAudioCaptor {
public:
    MockMicAudioCaptor() = default;
    ~MockMicAudioCaptor() override = default;

protected:
    const char* get_input_format_name() const override { return "mock_dshow"; }
    const char* get_device_name() const override { return ""; }
    void setup_options(AVDictionary**) override {}
};

class AudioManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto system = std::make_unique<MockSystemAudioCaptor>();
        auto mic = std::make_unique<MockMicAudioCaptor>();
        manager = std::make_unique<AudioManager>(std::move(system), std::move(mic));
    }

    std::unique_ptr<AudioManager> manager;
};

TEST_F(AudioManagerTest, ConstructorWorks) {
    EXPECT_NE(manager, nullptr);
    EXPECT_FALSE(manager->system_record_queue());
    EXPECT_FALSE(manager->mic_record_queue());
}

TEST_F(AudioManagerTest, EnableRecordingOnce) {
    manager->enable_recording_once();
    EXPECT_NE(manager->system_record_queue(), nullptr);
    EXPECT_NE(manager->mic_record_queue(), nullptr);
}

TEST_F(AudioManagerTest, DisableRecordingOnce) {
    manager->enable_recording_once();
    EXPECT_NE(manager->system_record_queue(), nullptr);
    EXPECT_NE(manager->mic_record_queue(), nullptr);

    manager->disable_recording_once();
    EXPECT_EQ(manager->system_record_queue(), nullptr);
    EXPECT_EQ(manager->mic_record_queue(), nullptr);
}

TEST_F(AudioManagerTest, RefCounting) {
    manager->enable_recording_once();
    manager->enable_recording_once();
    manager->enable_recording_once();
    EXPECT_NE(manager->system_record_queue(), nullptr);
    EXPECT_NE(manager->mic_record_queue(), nullptr);

    manager->disable_recording_once();
    manager->disable_recording_once();
    EXPECT_NE(manager->system_record_queue(), nullptr);
    EXPECT_NE(manager->mic_record_queue(), nullptr);

    manager->disable_recording_once();
    EXPECT_EQ(manager->system_record_queue(), nullptr);
    EXPECT_EQ(manager->mic_record_queue(), nullptr);
}

TEST_F(AudioManagerTest, StopAllWorks) {
    EXPECT_NO_FATAL_FAILURE(manager->stop_all());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}