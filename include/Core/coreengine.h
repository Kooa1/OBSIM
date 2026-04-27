//
// Created by 66 on 2026/4/27.
//

#ifndef OBSIM_LIBCORE_H
#define OBSIM_LIBCORE_H

#include "PCH.h"
#include "datasafequeue.h"
#include "Utils/ffmpegfactory.h"

class CoreEngine {
public:
    explicit CoreEngine();

private:
    std::unique_ptr<DataSafeQueue<AVFramePtr> > q_video;
};


#endif //OBSIM_LIBCORE_H
