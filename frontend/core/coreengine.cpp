//
// Created by 66 on 2026/4/27.
//

#include "coreengine.h"

CoreEngine::CoreEngine() {
    init_display_captor();
}

CoreEngine::~CoreEngine() {
    // video_thread.join();
}

void CoreEngine::run() {
    video_thread = std::thread([this]() {
        display_captor->start_capturing();
    });
}

void CoreEngine::init_display_captor() {
    display_captor = std::make_unique<DisplayCaptor>(q_video);
    q_video = std::make_unique<DataSafeQueue<AVFramePtr> >();
}

std::unique_ptr<DataSafeQueue<AVFramePtr> > &CoreEngine::get_queue() {
    return q_video;
}

std::unique_ptr<DisplayCaptor> &CoreEngine::get_captor() {
    return display_captor;
}
