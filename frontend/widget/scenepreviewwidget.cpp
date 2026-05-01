//
// Created by 66 on 2026/4/28.
//

#include "scenepreviewwidget.h"

#include <qevent.h>

ScenePreviewWidget::ScenePreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    // 请求模板缓冲
    QSurfaceFormat fmt = format();
    fmt.setStencilBufferSize(8);
    setFormat(fmt);

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
    rect1->base_width = 1920.0f;
    rect1->base_height = 1080.0f;
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
    rect2->base_width = 320.0f;
    rect2->base_height = 180.0f;
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

    // ===== 第1步：全局设置 =====
    glViewport(0, 0, w, h);
    glClearStencil(0);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // 窗口灰边背景
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // ===== 第2步：绘制画布黑色背景，模板值写入 2 =====
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 2, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(m_viewX, m_viewY);
    glVertex2f(m_viewX + m_viewW, m_viewY);
    glVertex2f(m_viewX + m_viewW, m_viewY + m_viewH);
    glVertex2f(m_viewX, m_viewY + m_viewH);
    glEnd();

    // ===== 第3步：仅增加源覆盖区域模板值（不写颜色） =====
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    glLoadIdentity();
    glTranslatef(m_viewX, m_viewY, 0);
    glScalef(m_viewW / CANVAS_W, m_viewH / CANVAS_H, 1);

    for (Source *source : m_scene.get_sources()) {
        if (!source || !source->visible) continue;
        QRectF bounds = source->get_bounding_rect();
        if (bounds.width() <= 0 || bounds.height() <= 0) continue;

        glPushMatrix();
        glTranslatef(source->pos_x, source->pos_y, 0.0f);
        glScalef(source->scale_x, source->scale_y, 1.0f);
        source->render();
        glPopMatrix();
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // ===== 第4步：正常渲染所有源 =====
    glDisable(GL_STENCIL_TEST);

    glLoadIdentity();
    glTranslatef(m_viewX, m_viewY, 0);
    glScalef(m_viewW / CANVAS_W, m_viewH / CANVAS_H, 1);

    for (Source *source : m_scene.get_sources()) {
        if (!source || !source->visible) continue;
        QRectF bounds = source->get_bounding_rect();
        if (bounds.width() <= 0 || bounds.height() <= 0) continue;

        glPushMatrix();
        glTranslatef(source->pos_x, source->pos_y, 0.0f);
        glScalef(source->scale_x, source->scale_y, 1.0f);
        source->render();
        glPopMatrix();
    }

    // 绘制选中框
    m_scene.render_selection_box();

    // ===== 第5步：在画布外源区域绘制马赛克覆盖 =====
    glLoadIdentity();  // 回到窗口像素坐标

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xFF);  // 只影响画布外源覆盖区
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // 马赛克参数
    const int blockSize = 16;  // 马赛克块大小（像素），越大马赛克越粗糙
    const int cols = (w + blockSize - 1) / blockSize;
    const int rows = (h + blockSize - 1) / blockSize;

    // 简单随机种子（固定种子保证每帧结果一致）
    srand(42);

    glBegin(GL_QUADS);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int x = col * blockSize;
            int y = row * blockSize;
            int bw = std::min(blockSize, w - x);
            int bh = std::min(blockSize, h - y);

            // 深灰到浅灰之间随机
            float gray = 0.3f + (rand() % 40) / 100.0f;  // 0.3 ~ 0.7 之间
            glColor3f(gray, gray, gray);

            glVertex2i(x, y);
            glVertex2i(x + bw, y);
            glVertex2i(x + bw, y + bh);
            glVertex2i(x, y + bh);
        }
    }
    glEnd();

    glDisable(GL_STENCIL_TEST);
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

void ScenePreviewWidget::update_cursor() {
    Qt::CursorShape shape = m_scene.get_cursor_for_current_pos();
    if (cursor().shape() != shape) {
        setCursor(shape);
    }
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
        update(); // 触发重绘
        update_cursor(); // ✅ 更新光标
    }
}

void ScenePreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    bool needs_update = m_scene.on_mouse_move(canvas_pos);
    if (needs_update) {
        update();
    }
    update_cursor(); // ✅ 每帧都更新光标
}

void ScenePreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_scene.on_mouse_release();
    update();
    update_cursor(); // ✅ 更新光标
}
