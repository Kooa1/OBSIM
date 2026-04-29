//
// Created by 66 on 2026/4/28.
//

#include "scenepreviewwidget.h"

ScenePreviewWidget::ScenePreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    init_CSS();
}

ScenePreviewWidget::~ScenePreviewWidget() {
}

void ScenePreviewWidget::init_CSS() {
    this->setStyleSheet("QWidget "
        "{ background : #808080;"
        " border-radius: 10px; }");
}

void ScenePreviewWidget::initializeGL() {
    QOpenGLWidget::initializeGL();
}

void ScenePreviewWidget::resizeGL(int w, int h) {
    QOpenGLWidget::resizeGL(w, h);
}

void ScenePreviewWidget::paintGL() {
    // 1. 获取当前窗口实际像素尺寸（启用高DPI后可能是逻辑像素×设备像素比）
    int w = width() * devicePixelRatio();
    int h = height() * devicePixelRatio();

    // 2. 计算等比缩放后的显示区域（保持 1920×1080）
    const float canvas_width = 1920.0f;
    const float canvas_height = 1080.0f;

    float scaleX = (float) w / canvas_width;
    float scaleY = (float) h / canvas_height;
    float scale = qMin(scaleX, scaleY); // 取较小缩放比，确保完整显示

    int view_width = (int) (canvas_width * scale);
    int view_height = (int) (canvas_height * scale);
    int view_x = (w - view_width) / 2; // 居中偏移
    int view_y = (h - view_height) / 2;

    // 3. 先清空整个窗口为黑色（非画面区域背景）
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 深灰/黑背景
    glClear(GL_COLOR_BUFFER_BIT);

    // 4. 设置“画面区域”的视口和裁剪
    glViewport(view_x, view_y, view_width, view_height);
    glScissor(view_x, view_y, view_width, view_height); // 后续绘制将严格限制在此矩形内
    glEnable(GL_SCISSOR_TEST);

    // 5. 再清一次画布区域为纯黑（真正的画布底色）
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 6. 设置正交投影，原点在左上角，Y 向下（与 Qt 坐标习惯一致）
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, canvas_width, canvas_height, 0, -1, 1); // 左、右、下、上（注意下是 canvas_height）

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // ---------- 这里开始遍历场景绘制所有源 ----------
    // 你的 Scene->Render() 会在这里调用，源的坐标可直接用 0~1920, 0~1080

    glDisable(GL_SCISSOR_TEST);
}
