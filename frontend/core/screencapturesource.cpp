//
// Created by 66 on 2026/5/8.
//

#include "screencapturesource.h"

ScreenCaptureSource::ScreenCaptureSource(int screen_index) {
    // ===== 1. 几何属性 =====
    base_width = 1920.0f;
    base_height = 1080.0f;
    pos_x = 0.0f;
    pos_y = 0.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
    visible = true;
    lock_aspect_ratio = true;
    fixed_aspect_ratio = 16.0f / 9.0f;

    // ===== 2. 创建采集类并启动采集线程 =====
    m_capture = std::make_unique<ScreenCaptor>();
    m_capture->start(); // 不阻塞，内部 detach 线程
}

ScreenCaptureSource::~ScreenCaptureSource() {
    if (m_capture) {
        m_capture->stop();
    }
}

// ========== Source 虚函数实现 ==========

void ScreenCaptureSource::load_resources() {
    // OpenGL 上下文已就绪，创建纹理（用默认分辨率）
    if (!m_texture_created) {
        create_texture(static_cast<int>(base_width), static_cast<int>(base_height));
    }
}

void ScreenCaptureSource::unload_resources() {
    if (m_texture_created) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_texture_created = false;
    }
}

void ScreenCaptureSource::render() {
    if (!m_texture_created || m_texture_id == 0) {
        // 降级：灰色占位矩形
        glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(base_width, 0.0f);
        glVertex2f(base_width, base_height);
        glVertex2f(0.0f, base_height);
        glEnd();
        return;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(base_width, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(base_width, base_height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, base_height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// ========== 纹理操作 ==========

// ============================================================================
// 函数：create_texture(int width, int height)
// 用途：创建 OpenGL 纹理对象，分配 GPU 内存
// 调用时机：load_resources() 或分辨率改变时
// 线程：必须在主线程（OpenGL 上下文线程）调用
// 参数：width, height - 纹理尺寸（像素）
// 注意：创建后纹理内容未定义（分配了内存但未填充数据），
//       后续通过 glTexSubImage2D 写入实际像素。
// ============================================================================
void ScreenCaptureSource::create_texture(int width, int height) {
    // 生成一个纹理 ID
    glGenTextures(1, &m_texture_id);

    // 绑定纹理为当前操作目标
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    // ---- 设置纹理采样参数 ----
    // 缩放过滤器：线性插值（放大或缩小时边缘平滑）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 纹理环绕模式：边缘钳位（不重复，超出区域的像素用边缘颜色填充）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 分配 GPU 纹理内存（内容未初始化，传 nullptr）
    // 格式 GL_RGBA8 对应每像素 4 字节（B、G、R、A 各 8 位）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    //                                            ^^^^^^^ 空指针表示仅分配，不填充

    // 记录当前纹理尺寸（用于后续尺寸变化检测）
    m_tex_width = width;
    m_tex_height = height;

    // 标记纹理已创建（防止重复创建，也可用于 render() 中的有效性判断）
    m_texture_created = true;
}

// ============================================================================
// 函数：upload_frame_to_texture(const AVFramePtr& frame)
// 用途：将 FFmpeg AVFrame 中的像素数据上传到 OpenGL 纹理
// 调用时机：由 update_texture_if_new_frame() 调用
// 线程：必须在主线程（OpenGL 上下文线程）调用
// 参数：frame - 包含 BGRA 像素数据的 FFmpeg 帧（通过移动语义传入）
// 注意：函数返回后 frame 析构，内存被释放。
//       但 glTexSubImage2D 是同步的，在函数返回前已完成数据拷贝到 GPU，
//       所以 frame 析构不影响纹理数据。
// ============================================================================
void ScreenCaptureSource::upload_frame_to_texture(const AVFramePtr &frame) {
    // 防御性检查：frame 必须有效
    if (!frame) return;

    int fw = frame->width; // 帧的实际宽度
    int fh = frame->height; // 帧的实际高度

    // ------------------------------------------------------------------------
    // 尺寸变化检测：如果帧的分辨率与当前纹理不匹配，重建纹理
    // 场景：屏幕分辨率动态改变（如显示器热插拔、分辨率切换）
    // ------------------------------------------------------------------------
    if (fw != m_tex_width || fh != m_tex_height) {
        glDeleteTextures(1, &m_texture_id); // 删除旧纹理
        create_texture(fw, fh); // 按新尺寸创建纹理
        // 同时更新 Source 基类的几何属性（保持纹理与画布坐标一致）
        base_width = static_cast<float>(fw);
        base_height = static_cast<float>(fh);
    }

    // 绑定当前纹理为操作目标
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    // ------------------------------------------------------------------------
    // 上传像素数据到纹理
    // 参数说明：
    //   target        = GL_TEXTURE_2D
    //   level         = 0（基础 mipmap 层级）
    //   xoffset/yoffset = 0, 0（从纹理的左上角开始）
    //   width/height  = 帧的实际尺寸
    //   format        = GL_BGRA（匹配 FFmpeg 的 AV_PIX_FMT_BGRA 格式）
    //   type          = GL_UNSIGNED_BYTE（每通道 8 位无符号整数）
    //   pixels        = frame->data[0]（指向 BGRA 像素数据的裸指针）
    // ------------------------------------------------------------------------
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    fw, fh,
                    GL_BGRA, GL_UNSIGNED_BYTE,
                    frame->data[0]);

    // 函数返回后，frame 析构（AVFrame 内存被 FFmpeg 释放）
    // data[0] 指向的内存也随之释放
}

// ========== 主线程每帧调用 ==========

// ============================================================================
// 函数：update_texture_if_new_frame()
// 用途：从采集类的安全队列中取出最新一帧，并上传到 OpenGL 纹理
// 调用时机：ScenePreviewWidget::paintGL() 每帧调用
// 线程：必须在主线程（OpenGL 上下文线程）调用
// 返回：true 表示本帧成功上传了新纹理数据，false 表示没有新帧
// ============================================================================
bool ScreenCaptureSource::update_texture_if_new_frame() {
    // ------------------------------------------------------------------------
    // 第一步：drain 安全队列，只保留最后一帧
    // 原理：采集线程可能已经 push 了多帧（如 30fps 采集，但渲染 60fps），
    //       队列中可能积压 2-3 帧。为降低延迟，丢弃所有旧帧，
    //       只保留最新的一帧用于上传。
    // ------------------------------------------------------------------------

    AVFramePtr latest; // 最终要上传的最新帧
    AVFramePtr temp; // 临时帧，用于 drain 队列

    bool got_any = false; // 标记：队列中是否有帧可取

    // 循环从队列取出所有帧，每次把取出的帧移动到 latest
    // 循环结束后，latest 持有队列中的最后一帧（最新的）
    while (m_capture->try_pop_frame(temp)) {
        latest = std::move(temp); // 移动语义：旧帧被析构，新帧接管
        got_any = true; // 标记：至少拿到了一帧
    }

    // ------------------------------------------------------------------------
    // 第二步：如果拿到了有效帧，上传到 OpenGL 纹理
    // ------------------------------------------------------------------------
    if (got_any && latest) {
        // latest 中的 AVFrame 包含：
        //   - data[0]：BGRA 像素数据（你的采集格式是 AV_PIX_FMT_BGRA）
        //   - width / height：帧的实际分辨率
        //   - linesize[0]：每行的字节跨度

        upload_frame_to_texture(std::move(latest));
        // 此处 latest 的所有权被转移给 upload_frame_to_texture，
        // 上传完成后自动析构，释放 FFmpeg 内存

        return true; // 本帧有纹理更新
    }

    return false; // 本帧没有新数据，纹理保持不变
}

void ScreenCaptureSource::set_frame_ready_callback(ScreenCaptor::FrameReadyCallback callback) {
    if (m_capture) {
        m_capture->set_frame_ready_callback(std::move(callback));
    }
}
