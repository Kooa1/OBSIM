//
// Created by 66 on 2026/4/28.
//

#include "scenepreviewwidget.h"

#include <qevent.h>

ScenePreviewWidget::ScenePreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    setMouseTracking(true);
    add_test_source();
}

ScenePreviewWidget::~ScenePreviewWidget() {
}

void ScenePreviewWidget::add_test_source() {
    // 创建红色矩形源
    auto rect1 = std::make_unique<TestSource>();
    rect1->pos_x = 200.0f;
    rect1->pos_y = 200.0f;
    rect1->base_width = 600.0f;
    rect1->base_height = 400.0f;
    rect1->scale_x = 1.0f;
    rect1->scale_y = 1.0f;
    rect1->color_r = 1.0f; // 红色
    rect1->color_g = 0.0f;
    rect1->color_b = 0.0f;
    rect1->color_a = 1.0f;

    // 创建绿色矩形源
    auto rect2 = std::make_unique<TestSource>();
    rect2->pos_x = 400.0f; // 与 rect1 有重叠区域
    rect2->pos_y = 300.0f;
    rect2->base_width = 500.0f;
    rect2->base_height = 400.0f;
    rect2->scale_x = 1.0f;
    rect2->scale_y = 1.0f;
    rect2->color_r = 0.0f; // 绿色
    rect2->color_g = 1.0f;
    rect2->color_b = 0.0f;
    rect2->color_a = 1.0f; // 完全不透明

    // 1️⃣ 先把裸指针注册给 Scene（在 move 之前）
    m_scene.add_source(rect1.get());
    m_scene.add_source(rect2.get());

    // 2️⃣ 只用一个容器管理生命周期
    m_sources_storage.push_back(std::move(rect1));
    m_sources_storage.push_back(std::move(rect2));

    qDebug() << "添加了" << m_scene.get_sources().size() << "个测试源";
}

void ScenePreviewWidget::rendering_view() {
    const int w = this->width() * devicePixelRatio();
    const int h = this->height() * devicePixelRatio();

    const float scaleX = static_cast<float>(w) / CANVAS_W;
    const float scaleY = static_cast<float>(h) / CANVAS_H;
    const float scale = qMin(scaleX, scaleY);

    m_viewW = static_cast<int>(CANVAS_W * scale);
    m_viewH = static_cast<int>(CANVAS_H * scale);
    m_viewX = (w - m_viewW) / 2;
    m_viewY = (h - m_viewH) / 2;

    // ===== 第1步：清空整个窗口（灰色背景） =====
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ===== 第2步：设置画布区域 =====
    glViewport(m_viewX, m_viewY, m_viewW, m_viewH);
    glScissor(m_viewX, m_viewY, m_viewW, m_viewH);
    glEnable(GL_SCISSOR_TEST);

    // 清空画布为黑色
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ===== 第3步：设置正交投影 =====
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, CANVAS_W, CANVAS_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    // ===== 第4步：渲染所有源 =====
    int rendered_count = 0;
    for (Source *source: m_scene.get_sources()) {
        // 🔧 修复：不可见或空指针时跳过
        if (!source || !source->visible) {
            qDebug() << "跳过源：!source=" << (!source)
                    << " visible=" << (source ? source->visible : false);
            continue;
        }

        // 获取源的边界矩形
        QRectF bounds = source->get_bounding_rect();
        qDebug() << "尝试渲染源，bounds:" << bounds
                << " base:" << source->base_width << "x" << source->base_height;

        // 🔧 修复：跳过零尺寸的源
        if (bounds.width() <= 0 || bounds.height() <= 0) {
            qDebug() << "跳过零尺寸源";
            continue;
        }

        // 跳过完全在画布外的源
        if (bounds.left() >= CANVAS_W || bounds.right() <= 0 ||
            bounds.top() >= CANVAS_H || bounds.bottom() <= 0) {
            qDebug() << "跳过画布外源：" << bounds;
            continue;
        }

        glLoadIdentity();

        // 应用平移
        glTranslatef(source->pos_x, source->pos_y, 0.0f);

        // 应用缩放
        glScalef(source->scale_x, source->scale_y, 1.0f);

        // 渲染（此时原点在源左上角）
        source->render();
        rendered_count++;
    }

    glLoadIdentity();
    m_scene.render_selection_box();

    glDisable(GL_SCISSOR_TEST);
}

QPointF ScenePreviewWidget::screen_to_canvas(const QPointF &screen_pos) const {
    // screen_pos 是逻辑像素（Qt6 event->position() 返回值）
    // 需要乘以 dpr 得到物理像素，再映射到 1920x1080 画布
    const qreal dpr = devicePixelRatio();
    const qreal physX = screen_pos.x() * dpr;
    const qreal physY = screen_pos.y() * dpr;

    const double logicX = (physX - m_viewX) / m_viewW * CANVAS_W;
    const double logicY = (physY - m_viewY) / m_viewH * CANVAS_H;

    return QPointF(logicX, logicY);
}

void ScenePreviewWidget::calculate_pos(QPointF point_f) {
    const qreal dpr = devicePixelRatio();
    const qreal physX = point_f.x() * dpr;
    const qreal physY = point_f.y() * dpr;

    const double logicX = (physX - m_viewX) / m_viewW * CANVAS_W;
    const double logicY = (physY - m_viewY) / m_viewH * CANVAS_H;

    // 距离边缘的像素数（逻辑坐标系）
    const int distToLeft = qRound(logicX);
    const int distToTop = qRound(logicY);
    const int distToRight = qRound(CANVAS_W - logicX);
    const int distToBottom = qRound(CANVAS_H - logicY);

    qDebug() << QString("逻辑坐标: (%1, %2)  左:%3 上:%4 右:%5 下:%6")
            .arg(logicX, 0, 'f', 1)
            .arg(logicY, 0, 'f', 1)
            .arg(distToLeft).arg(distToTop)
            .arg(distToRight).arg(distToBottom);
}

void ScenePreviewWidget::initializeGL() {
    QOpenGLWidget::initializeGL();
}

void ScenePreviewWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void ScenePreviewWidget::paintGL() {
    rendering_view();
}

void ScenePreviewWidget::mousePressEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    bool handled = m_scene.on_mouse_press(canvas_pos);
    if (handled) {
        update(); // 触发重绘显示选中框
    }
}

void ScenePreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    // calculate_pos(event->position());
    // QOpenGLWidget::mouseMoveEvent(event);

    QPointF canvas_pos = screen_to_canvas(event->position());
    bool needs_update = m_scene.on_mouse_move(canvas_pos);
    if (needs_update) {
        update();
    }
}

void ScenePreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_scene.on_mouse_release();
    update();
}
