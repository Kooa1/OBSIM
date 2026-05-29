#ifndef OBSIM_CONFIGMANAGER_H
#define OBSIM_CONFIGMANAGER_H

#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrentRun>
#include <QMutex>
#include <functional>

/// @brief Create a QSettings instance pointing to config.ini in the application directory
inline QSettings app_settings() {
    return QSettings(
        QDir(QCoreApplication::applicationDirPath()).filePath("config.ini"),
        QSettings::IniFormat
    );
}

/// @brief Asynchronous QSettings read/write with a shared mutex
namespace AsyncSettings {

/// @brief Global mutex serializing QSettings file I/O
inline QMutex& io_mutex() {
    static QMutex mutex;
    return mutex;
}

/// @brief Asynchronously write to settings
inline QFuture<void> async_save(std::function<void(QSettings&)> writer) {
    return QtConcurrent::run([w = std::move(writer)]() {
        QMutexLocker locker(&io_mutex());
        QSettings s = app_settings();
        w(s);
        s.sync();
    });
}

/// @brief Asynchronously read from settings
template<typename T>
inline QFuture<T> async_load(std::function<T(QSettings&)> reader) {
    return QtConcurrent::run([r = std::move(reader)]() {
        QMutexLocker locker(&io_mutex());
        QSettings s = app_settings();
        return r(s);
    });
}

} // namespace AsyncSettings

#endif