//
// Created by 66 on 2025/12/18.
//

// #include <vld.h>
#include <QApplication>
#include <QMainWindow>

#include "captor.h"

extern "C" {
#include <libavdevice/avdevice.h>
}

int main(int argc, char *argv[]) {

    const std::string rtmp_url = "rtemp://172.28.206.198/live/stream_key";

    avdevice_register_all();

    QApplication a(argc, argv);

    const auto captor = std::make_unique<Captor>();

    return QApplication::exec();
}