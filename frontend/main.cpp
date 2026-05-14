//
// Created by 66 on 2025/12/18.
//

// #include <vld.h>
#include <QApplication>

#include "utils/PCH.h"
#include "widget/mainwindow.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

void init_FFMPEG() {
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

int main(int argc, char *argv[]) {
    // 推流链接
    const std::string rtmp_url = "rtemp://172.28.206.198/live/stream_key";

    init_FFMPEG();

    QApplication a(argc, argv);

    auto main_window = new MainWindow();

    // return QApplication::exec();
    a.exec();

    // obs_shutdown();
    return 0;
}
