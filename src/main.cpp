//
// Created by 66 on 2025/12/18.
//

// #include <vld.h>
#include <QApplication>

#include "utils/PCH.h"
#include "frontend/widget/mainwindow.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

void init_QSettings() {
    QCoreApplication::setOrganizationName("OBSIM");
    QCoreApplication::setOrganizationDomain("obsim.app");
    QCoreApplication::setApplicationName("OBSIM");
}

void init_FFMPEG() {
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

int main(int argc, char *argv[]) {
    init_QSettings();
    init_FFMPEG();

    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication a(argc, argv);

    auto main_window = new MainWindow();

    return QApplication::exec();

    return 0;
}
