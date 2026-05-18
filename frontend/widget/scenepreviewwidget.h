#ifndef OBSIM_SCENEPREVIEWWIDGET_H
#define OBSIM_SCENEPREVIEWWIDGET_H

#include <QDebug>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QMouseEvent>
#include <QPointer>
#include <atomic>
#include <memory>

#include "core/source.h"
#include "core/scene.h"
#include "test/testsource.h"
#include "core/videocaptor.h"
#include "core/screencapturesource.h"
#include "core/cameracapturesource.h"
#include "core/textsource.h"
#include "utils/devicemanager.h"


class ScenePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit ScenePreviewWidget(QWidget *parent = nullptr);

    ~ScenePreviewWidget() override;

    void add_test_source();

    void add_screen_capture_source(const CaptorConfig &config);

    void add_camera_capture_source(std::string device_description = "");

    void add_text_source(const QString &text, const QFont &font, const QColor &color);

    void remove_source(int index);

    void select_source_at(int index);

signals:
    void canvas_selection_changed(int index);

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
    static constexpr float CANVAS_W = 1920.0f;
    static constexpr float CANVAS_H = 1080.0f;

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
};

#endif
