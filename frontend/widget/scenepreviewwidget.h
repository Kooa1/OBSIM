//
// Created by 66 on 2026/4/28.
//

#ifndef OBSIM_SCENEPREVIEWWIDGET_H
#define OBSIM_SCENEPREVIEWWIDGET_H

#include <QtOpenGLWidgets/QOpenGLWidget>

class ScenePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit ScenePreviewWidget(QWidget *parent = nullptr);

    ~ScenePreviewWidget() override;

    void init_CSS();

protected:
    void initializeGL() override;

    void resizeGL(int w, int h) override;

    void paintGL() override;
};


#endif //OBSIM_SCENEPREVIEWWIDGET_H
