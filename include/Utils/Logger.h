//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_LOGER_H
#define OBSIM_LOGER_H

#include <iostream>

#include "av_err2str_cxx.h"

extern "C" {
#include "libavutil/log.h"
}

class Logger {
public:
    enum class Level {
        Debug = AV_LOG_DEBUG,
        Info = AV_LOG_INFO,
        Error = AV_LOG_ERROR
    };

    explicit Logger(Level);

    ~Logger();

    static void log(Level level, const std::string &str, const int ret);
};

#endif //OBSIM_LOGER_H
