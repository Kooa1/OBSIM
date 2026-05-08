//
// Created by 66 on 2026/5/8.
//

#include "screencapturesource.h"

ScreenCaptureSource::ScreenCaptureSource(int screen_index) {
    // ===== 1. 设置 Source 的几何属性 =====
    base_width = 1920.0f;
    base_height = 1080.0f;
    pos_x = 0.0f;
    pos_y = 0.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
    visible = true;
    lock_aspect_ratio = true;
    fixed_aspect_ratio = 16.0f / 9.0f;

    // ===== 2. 创建你的采集类实例并启动 =====
    m_capture = std::make_unique<ScreenCaptor>(screen_index);
    m_capture->start_capturing(); // 采集类内部启动线程，开始采集
}
