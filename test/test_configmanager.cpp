#include <gtest/gtest.h>

#include <QApplication>
#include <QSettings>
#include <QTemporaryFile>
#include <QDir>

#include "utils/configmanager.h"

int argc = 1;
char argv0[] = "test_configmanager";
char* argv[] = { argv0, nullptr };
QApplication app(argc, argv);

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        QSettings s = app_settings();
        s.clear();
        s.sync();
    }
};

TEST_F(ConfigManagerTest, DefaultSettingsExist) {
    QSettings s = app_settings();
    EXPECT_EQ(s.format(), QSettings::IniFormat);
    EXPECT_FALSE(s.fileName().isEmpty());
}

TEST_F(ConfigManagerTest, WriteAndReadSetting) {
    {
        QSettings s = app_settings();
        s.setValue("TestSection/TestKey", "TestValue");
        s.sync();
    }
    {
        QSettings s = app_settings();
        QString value = s.value("TestSection/TestKey").toString();
        EXPECT_EQ(value, "TestValue");
    }
}

TEST_F(ConfigManagerTest, SettingPersistence) {
    QSettings s1 = app_settings();
    s1.setValue("PersistenceTest/Key", "PersistentValue");
    s1.sync();

    QSettings s2 = app_settings();
    QString value = s2.value("PersistenceTest/Key").toString();
    EXPECT_EQ(value, "PersistentValue");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}