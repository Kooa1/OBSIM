//
// Created by 66 on 2026/4/27.
//

#ifndef OBSIM_LIBCORE_H
#define OBSIM_LIBCORE_H

#include <thread>

#include "../utils/PCH.h"
#include "../utils/datasafequeue.h"
#include "../utils/ffmpegfactory.h"
#include "screencaptor.h"

class CoreEngine {
public:
    explicit CoreEngine();

    ~CoreEngine();

    void run();

    void init_display_captor();

    std::unique_ptr<DataSafeQueue<AVFramePtr> > &get_queue();

    std::unique_ptr<ScreenCaptor> &get_captor();

private:
    std::unique_ptr<DataSafeQueue<AVFramePtr> > q_video;

    std::unique_ptr<ScreenCaptor> display_captor;

    std::thread video_thread;
};


#endif //OBSIM_LIBCORE_H
