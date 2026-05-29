#ifndef ALOG_AV_ERR2STR_CXX_H
#define ALOG_AV_ERR2STR_CXX_H

#include <iostream>
#include <string>

extern "C"{
#include "libavutil/error.h"
};

/// @brief Convert FFmpeg error code to string
/// @param errnum FFmpeg error code
/// @return Error description string
inline std::string av_error_cxx(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

#endif //ALOG_AV_ERR2STR_CXX_H
