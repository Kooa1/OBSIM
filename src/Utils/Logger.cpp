//
// Created by 66 on 2026/4/26.
//

#include "Utils/Logger.h"


Logger::Logger(Level level) {
    av_log_set_level(static_cast<int>(level));
}

Logger::~Logger() {
}

void Logger::log(Level level, const std::string &str, const int ret) {
    av_log(nullptr, static_cast<int>(level), str.c_str(), av_error_cxx(ret).c_str());
}
