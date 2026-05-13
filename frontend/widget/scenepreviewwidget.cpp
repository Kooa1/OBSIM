#include "scenepreviewwidget.h"

ScenePreviewWidget::ScenePreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    // 请求模板缓冲
    QSurfaceFormat fmt = format();
    fmt.setStencilBufferSize(8);
    setFormat(fmt);

    setMouseTracking(true);

    // 保存 QPointer 用于跨线程安全
    m_self_guard = this;

    // 测试源
    // add_test_source();
    // add_screen_capture_source(0);
    // add_camera_capture_source();
}

ScenePreviewWidget::~ScenePreviewWidget() {
    m_self_guard.clear(); // 标记 Widget 已销毁

    makeCurrent();
    // 卸载所有源的 OpenGL 资源
    for (Source *src : m_scene.get_sources()) {
        src->unload_resources();
    }
    delete_mosaic_list();
    doneCurrent();
}

void ScenePreviewWidget::add_test_source() {
    auto rect1 = std::make_unique<TestSource>();
    rect1->pos_x = 200.0f;
    rect1->pos_y = 200.0f;
    rect1->base_width = 1920.0f;
    rect1->base_height = 1080.0f;
    rect1->scale_x = 1.0f;
    rect1->scale_y = 1.0f;
    rect1->color_r = 1.0f;
    rect1->color_g = 0.0f;
    rect1->color_b = 0.0f;
    rect1->color_a = 1.0f;

    auto rect2 = std::make_unique<TestSource>();
    rect2->pos_x = 400.0f;
    rect2->pos_y = 300.0f;
    rect2->base_width = 320.0f;
    rect2->base_height = 180.0f;
    rect2->scale_x = 1.0f;
    rect2->scale_y = 1.0f;
    rect2->color_r = 0.0f;
    rect2->color_g = 1.0f;
    rect2->color_b = 0.0f;
    rect2->color_a = 1.0f;

    m_scene.add_source(rect1.get());
    m_scene.add_source(rect2.get());

    m_sources_storage.push_back(std::move(rect1));
    m_sources_storage.push_back(std::move(rect2));

}

void ScenePreviewWidget::add_screen_capture_source(int screen_index) {
    auto src = std::make_unique<ScreenCaptureSource>(screen_index);

    // ✅ 使用 QPointer + invokeMethod 安全跨线程投递
    QPointer<ScenePreviewWidget> self = m_self_guard;
    src->set_frame_ready_callback([self]() {
        if (self) {
            QMetaObject::invokeMethod(self, [self]() {
                self->update();
            }, Qt::QueuedConnection);
        }
    });

    m_scene.add_source(src.get());
    m_sources_storage.push_back(std::move(src));
}

void ScenePreviewWidget::add_camera_capture_source() {
    auto src = std::make_unique<CameraCaptureSource>();

    QPointer<ScenePreviewWidget> self = m_self_guard;
    src->set_frame_ready_callback([self]() {
        if (self) {
            QMetaObject::invokeMethod(self, [self]() {
                self->update();
            }, Qt::QueuedConnection);
        }
    });

    m_scene.add_source(src.get());
    m_sources_storage.push_back(std::move(src));
}

void ScenePreviewWidget::setup_viewport_and_clear() {
    const int w = this->width() * devicePixelRatio();
    const int h = this->height() * devicePixelRatio();

    const float scaleX = static_cast<float>(w) / CANVAS_W;
    const float scaleY = static_cast<float>(h) / CANVAS_H;
    const float scale = qMin(scaleX, scaleY);

    m_viewW = static_cast<int>(CANVAS_W * scale);
    m_viewH = static_cast<int>(CANVAS_H * scale);
    m_viewX = (w - m_viewW) / 2;
    m_viewY = (h - m_viewH) / 2;

    glViewport(0, 0, w, h);
    glClearStencil(0);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void ScenePreviewWidget::render_canvas_background() {
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
}

void ScenePreviewWidget::render_source_stencil_for_selected() {
    Source *selected = m_scene.selected_source();
    if (!selected || !selected->visible) return;

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // 3a. 选中源覆盖区域：模板值设为 1
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glLoadIdentity();
    glTranslatef(m_viewX, m_viewY, 0);
    glScalef(m_viewW / CANVAS_W, m_viewH / CANVAS_H, 1);

    QRectF bounds = selected->get_bounding_rect();
    if (bounds.width() > 0 && bounds.height() > 0) {
        glPushMatrix();
        glTranslatef(selected->pos_x, selected->pos_y, 0.0f);
        glScalef(selected->scale_x, selected->scale_y, 1.0f);
        selected->render();
        glPopMatrix();
    }

    // 3b. 画布矩形区域：将画布内的模板值恢复为 2
    glLoadIdentity();

    glStencilFunc(GL_ALWAYS, 2, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glBegin(GL_QUADS);
    glVertex2f(m_viewX, m_viewY);
    glVertex2f(m_viewX + m_viewW, m_viewY);
    glVertex2f(m_viewX + m_viewW, m_viewY + m_viewH);
    glVertex2f(m_viewX, m_viewY + m_viewH);
    glEnd();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void ScenePreviewWidget::render_all_sources_with_clip() {
    const int h = this->height() * devicePixelRatio();

    glDisable(GL_STENCIL_TEST);

    glLoadIdentity();
    glTranslatef(m_viewX, m_viewY, 0);
    glScalef(m_viewW / CANVAS_W, m_viewH / CANVAS_H, 1);

    glEnable(GL_SCISSOR_TEST);
    glScissor(m_viewX, h - (m_viewY + m_viewH), m_viewW, m_viewH);

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

    glDisable(GL_SCISSOR_TEST);

    m_scene.render_selection_box();
}

void ScenePreviewWidget::render_mosaic_for_selected() {
    if (!m_scene.selected_source()) return;

    const int w = this->width() * devicePixelRatio();
    const int h = this->height() * devicePixelRatio();

    glLoadIdentity();

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    ensure_mosaic_list(w, h);
    glCallList(m_mosaic_list);

    glDisable(GL_STENCIL_TEST);
}

void ScenePreviewWidget::rendering_view() {
    setup_viewport_and_clear();
    render_canvas_background();
    render_source_stencil_for_selected();
    render_all_sources_with_clip();
    render_mosaic_for_selected();
}

void ScenePreviewWidget::update_all_video_sources() {
    for (Source *src : m_scene.get_sources()) {
        src->update_frame();
    }
}

QPointF ScenePreviewWidget::screen_to_canvas(const QPointF &screen_pos) const {
    const qreal dpr = devicePixelRatio();
    const qreal physX = screen_pos.x() * dpr;
    const qreal physY = screen_pos.y() * dpr;

    const double logicX = (physX - m_viewX) / m_viewW * CANVAS_W;
    const double logicY = (physY - m_viewY) / m_viewH * CANVAS_H;

    return QPointF(logicX, logicY);
}


void ScenePreviewWidget::update_cursor() {
    Qt::CursorShape shape = m_scene.get_cursor_for_current_pos();
    if (cursor().shape() != shape) {
        setCursor(shape);
    }
}

void ScenePreviewWidget::ensure_mosaic_list(int w, int h) {
    if (m_mosaic_list != 0 && m_mosaic_w == w && m_mosaic_h == h)
        return;

    delete_mosaic_list();
    m_mosaic_list = glGenLists(1);
    m_mosaic_w = w;
    m_mosaic_h = h;

    const int blockSize = 16;
    const int cols = (w + blockSize - 1) / blockSize;
    const int rows = (h + blockSize - 1) / blockSize;
    srand(42);

    glNewList(m_mosaic_list, GL_COMPILE);
    glBegin(GL_QUADS);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int x = col * blockSize;
            int y = row * blockSize;
            int bw = std::min(blockSize, w - x);
            int bh = std::min(blockSize, h - y);
            float gray = 0.3f + (rand() % 40) / 100.0f;
            glColor3f(gray, gray, gray);
            glVertex2i(x, y);
            glVertex2i(x + bw, y);
            glVertex2i(x + bw, y + bh);
            glVertex2i(x, y + bh);
        }
    }
    glEnd();
    glEndList();
}

void ScenePreviewWidget::delete_mosaic_list() {
    if (m_mosaic_list != 0) {
        glDeleteLists(m_mosaic_list, 1);
        m_mosaic_list = 0;
    }
}

void ScenePreviewWidget::initializeGL() {
    QOpenGLWidget::initializeGL();
    for (Source *src : m_scene.get_sources()) {
        src->load_resources();
    }
}

void ScenePreviewWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
    delete_mosaic_list();
}

void ScenePreviewWidget::paintGL() {
    update_all_video_sources();
    rendering_view();
}

void ScenePreviewWidget::mousePressEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    bool handled = m_scene.on_mouse_press(canvas_pos);
    if (handled) {
        update();
        update_cursor();
    }
}

void ScenePreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    bool needs_update = m_scene.on_mouse_move(canvas_pos);
    if (needs_update) {
        update();
    }
    update_cursor();
}

void ScenePreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_scene.on_mouse_release();
    update();
    update_cursor();
}