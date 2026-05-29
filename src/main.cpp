#include <QApplication>
#include <QFile>

#include "utils/PCH.h"
#include "frontend/widget/mainwindow.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

/// @brief Registers application metadata for QSettings persistence.
void init_QSettings() {
    QCoreApplication::setOrganizationName("OBSIM");
    QCoreApplication::setOrganizationDomain("obsim.app");
    QCoreApplication::setApplicationName("OBSIM");
}

/// @brief Registers FFmpeg libavdevice and sets log level to error-only.
void init_FFMPEG() {
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

/// @brief Loads and applies the global QSS stylesheet from Qt resources.
void load_global_stylesheet() {
    QFile style_file(":/style.qss");
    if (style_file.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QString::fromUtf8(style_file.readAll());
        qApp->setStyleSheet(style);
        style_file.close();
    }
}

int main(int argc, char *argv[]) {
    init_QSettings();
    init_FFMPEG();

    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication a(argc, argv);

    MainWindow main_window;
    load_global_stylesheet();

    return QApplication::exec();

    return 0;
}