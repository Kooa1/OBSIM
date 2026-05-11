//
// Created by 66 on 2026/4/28.
//

#ifndef OBSIM_SCENEPREVIEWWIDGET_H
#define OBSIM_SCENEPREVIEWWIDGET_H

#include <iostream>

#include <QDebug>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include "core/source.h"
#include "core/scene.h"
#include "test/testsource.h"
#include "core/screencaptor.h"
#include "core/screencapturesource.h"
#include "core/cameracapturesource.h"


class ScenePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit ScenePreviewWidget(QWidget *parent = nullptr);

    ~ScenePreviewWidget() override;

    void add_test_source();

    void add_screen_capture_source(int screen_index);

    void add_camera_capture_source();

private:
    void rendering_view();

    void update_all_video_sources();

    QPointF screen_to_canvas(const QPointF &screen_pos) const;

    void calculate_pos(QPointF point_f);

    void update_cursor();

    void on_frame_ready();

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

    GLuint m_mosaic_list = 0; // 马赛克显示列表 ID
    int m_mosaic_w = 0; // 创建列表时的视图宽
    int m_mosaic_h = 0; // 创建列表时的视图高

    std::vector<std::unique_ptr<Source> > m_sources_storage;
    Scene m_scene;
};


#endif //OBSIM_SCENEPREVIEWWIDGET_H
