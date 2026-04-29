//
// Created by 66 on 2025/12/18.
//

// #include <vld.h>
#include <QApplication>

#include "utils/PCH.h"
#include "widget/mainwindow.h"
#include "core/coreengine.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

// void init_OBS() {
//     // 应用启动
//     if (!obs_startup("en-US", nullptr, nullptr)) {
//         qCritical() << "Failed to initialize libobs";
//         return;
//     }
//
//     // 加载 OpenGL 渲染模块（必须）
//     obs_load_all_modules(); // 加载所有插件模块
// }

void init_FFMPEG() {
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

int main(int argc, char *argv[]) {
    // 推流链接
    const std::string rtmp_url = "rtemp://172.28.206.198/live/stream_key";

    init_FFMPEG();

    QApplication a(argc, argv);

    // init_OBS();

    auto core_engine = std::make_unique<CoreEngine>();
    auto main_window = new MainWindow(*core_engine);

    // core_engine->run();

    // return QApplication::exec();
    a.exec();

    // obs_shutdown();
    return 0;
}
