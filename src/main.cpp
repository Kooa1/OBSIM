//
// Created by 66 on 2025/12/18.
//

// #include <vld.h>
#include <QApplication>

#include "../include/PCH.h"
#include "UI/mainwindow.h"
#include "Core/coreengine.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

int main(int argc, char *argv[]) {
    const std::string rtmp_url = "rtemp://172.28.206.198/live/stream_key";

    avdevice_register_all();
    av_log_set_level(AV_LOG_DEBUG);

    QApplication a(argc, argv);

    auto core_engine = std::make_unique<CoreEngine>();
    auto main_window = new MainWindow(*core_engine);

    return QApplication::exec();
}
