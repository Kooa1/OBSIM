#ifndef OBSIM_SCENEPREVIEWWIDGET_H
#define OBSIM_SCENEPREVIEWWIDGET_H

#include <QDebug>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>
#include <chrono>

#include "../base/source.h"
#include "core/scene.h"
#include "test/testsource.h"
#include "../base/videocaptor.h"
#include "core/screencapturesource.h"
#include "core/cameracapturesource.h"
#include "core/textsource.h"
#include "core/imagesource.h"
#include "utils/devicemanager.h"


class ScenePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit ScenePreviewWidget(QWidget *parent = nullptr);

    ~ScenePreviewWidget() override;

    void add_test_source();

    void add_screen_capture_source(const CaptorConfig &config, const QString &name = QString());

    void add_camera_capture_source(const QString &name, std::string device_description = "");

    void add_text_source(const QString &text, const QFont &font, const QColor &color, const QString &name = QString());

    void add_image_source(const QString &file_path, const QString &name = QString());

    void remove_source(int index);

    void select_source_at(int index);

    void move_source_up(int index);
    void move_source_down(int index);

    // 录制帧捕获回调（data: BGRA, stride: width*4）
    using FrameCaptureCallback = std::function<void(const uint8_t *data, int stride, int w, int h)>;
    void set_frame_capture_callback(FrameCaptureCallback cb) { m_frame_capture_callback = std::move(cb); }

    static constexpr float CANVAS_W = 1920.0f;
    static constexpr float CANVAS_H = 1080.0f;

    size_t source_count() const { return m_sources_storage.size(); }
    Source* source_at(size_t index) const { return m_sources_storage[index].get(); }

signals:
    void canvas_selection_changed(int index);
    void source_position_changed();

private:
    void rendering_view();

    void setup_viewport_and_clear();

    void render_canvas_background();

    void render_source_stencil_for_selected();

    void render_all_sources_with_clip();

    void render_mosaic_for_selected();

    void update_all_video_sources();

    QPointF screen_to_canvas(const QPointF &screen_pos) const;

    void update_cursor();

    void ensure_mosaic_list(int w, int h);

    void delete_mosaic_list();

protected:
    void initializeGL() override;

    void resizeGL(int w, int h) override;

    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_viewX = 0;
    int m_viewY = 0;
    int m_viewW = 1920;
    int m_viewH = 1080;

    GLuint m_mosaic_list = 0;
    int m_mosaic_w = 0;
    int m_mosaic_h = 0;

    std::vector<std::unique_ptr<Source> > m_sources_storage;
    Scene m_scene;

    QPointer<ScenePreviewWidget> m_self_guard; // 跨线程安全引用
    QTimer *m_render_timer = nullptr;           // 60fps 渲染定时器

    FrameCaptureCallback m_frame_capture_callback;
    std::chrono::steady_clock::time_point m_last_capture_time;
    int m_capture_fps = 30;
};

#endif
