#ifndef OBSIM_CONFIGMANAGER_H
#define OBSIM_CONFIGMANAGER_H

#include <QSettings>
#include <QDir>
#include <QCoreApplication>

inline QSettings app_settings() {
    return QSettings(
        QDir(QCoreApplication::applicationDirPath()).filePath("config.ini"),
        QSettings::IniFormat
    );
}

#endif