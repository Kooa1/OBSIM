#include "scenepreviewwidget.h"

ScenePreviewWidget::ScenePreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);

    QSurfaceFormat fmt = format();
    fmt.setStencilBufferSize(8);
    fmt.setSwapInterval(1);
    setFormat(fmt);

    setMouseTracking(true);

    m_self_guard = this;

    m_render_timer = new QTimer(this);
    connect(m_render_timer, &QTimer::timeout, this, [this]() {
        update();
    });
    m_render_timer->setTimerType(Qt::PreciseTimer);
    m_render_timer->start(16);

    add_scene("场景 1");
}

ScenePreviewWidget::~ScenePreviewWidget() {
    m_self_guard.clear();

    makeCurrent();
    for (auto &sd : m_scene_list) {
        for (Source *src : sd.scene.get_sources()) {
            src->unload_resources();
        }
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

    current_scene().add_source(rect1.get());
    current_scene().add_source(rect2.get());

    current_storage().push_back(std::move(rect1));
    current_storage().push_back(std::move(rect2));
}

void ScenePreviewWidget::add_screen_capture_source(const CaptorConfig &config, const QString &name) {
    auto src = std::make_unique<ScreenCaptureSource>(config);
    src->display_name = name;
    current_scene().add_source(src.get());
    current_storage().push_back(std::move(src));
}

void ScenePreviewWidget::add_camera_capture_source(const QString &name, std::string device_description) {
    auto src = std::make_unique<CameraCaptureSource>(std::move(device_description));
    src->display_name = name;
    current_scene().add_source(src.get());
    current_storage().push_back(std::move(src));
}

void ScenePreviewWidget::add_text_source(const QString &text, const QFont &font, const QColor &color, const QString &name) {
    auto src = std::make_unique<TextSource>();
    src->display_name = name;
    src->set_text(text);
    src->set_font(font);
    src->set_color(color);
    src->pos_x = 200.0f;
    src->pos_y = 200.0f;

    current_scene().add_source(src.get());
    current_storage().push_back(std::move(src));
}

void ScenePreviewWidget::add_image_source(const QString &file_path, const QString &name) {
    auto src = std::make_unique<ImageSource>(file_path);
    src->display_name = name;
    src->pos_x = 200.0f;
    src->pos_y = 200.0f;

    current_scene().add_source(src.get());
    current_storage().push_back(std::move(src));
}

void ScenePreviewWidget::select_source_at(int index) {
    auto &storage = current_storage();
    if (index < 0 || index >= static_cast<int>(storage.size())) return;
    current_scene().set_selected_source(storage[index].get());
    current_scene().on_mouse_release();
}

void ScenePreviewWidget::remove_source(int index) {
    auto &storage = current_storage();
    if (index < 0 || index >= static_cast<int>(storage.size())) return;

    auto *video_src = dynamic_cast<VideoSource *>(storage[index].get());
    if (video_src) {
        video_src->stop_capture();
    }

    Source *src = storage[index].get();
    bool was_selected = (current_scene().selected_source() == src);
    if (was_selected) {
        current_scene().clear_selection();
    }
    current_scene().remove_source(src);
    storage.erase(storage.begin() + index);
    if (was_selected) {
        emit canvas_selection_changed(-1);
    }
}

void ScenePreviewWidget::move_source_up(int index) {
    auto &storage = current_storage();
    if (index < 0 || index + 1 >= static_cast<int>(storage.size())) return;

    Source *src = storage[index].get();
    current_scene().move_up(src);
    std::swap(storage[index], storage[index + 1]);
}

void ScenePreviewWidget::move_source_down(int index) {
    auto &storage = current_storage();
    if (index <= 0 || index >= static_cast<int>(storage.size())) return;

    Source *src = storage[index].get();
    current_scene().move_down(src);
    std::swap(storage[index], storage[index - 1]);
}

size_t ScenePreviewWidget::source_count() const {
    return current_storage().size();
}

Source* ScenePreviewWidget::source_at(size_t index) const {
    return current_storage()[index].get();
}

int ScenePreviewWidget::add_scene(const QString &name) {
    SceneData sd;
    sd.name = name;
    m_scene_list.push_back(std::move(sd));
    return static_cast<int>(m_scene_list.size()) - 1;
}

QString ScenePreviewWidget::scene_name_at(int index) const {
    if (index < 0 || index >= static_cast<int>(m_scene_list.size())) return {};
    return m_scene_list[index].name;
}

void ScenePreviewWidget::clear_all_scenes() {
    for (auto &sd : m_scene_list) {
        for (auto &src : sd.source_storage) {
            auto *video_src = dynamic_cast<VideoSource *>(src.get());
            if (video_src) {
                video_src->stop_capture();
            }
        }
    }
    m_scene_list.clear();
    m_current_scene_index = 0;
}

void ScenePreviewWidget::remove_scene(int index) {
    if (index < 0 || index >= static_cast<int>(m_scene_list.size())) return;

    for (auto &src : m_scene_list[index].source_storage) {
        auto *video_src = dynamic_cast<VideoSource *>(src.get());
        if (video_src) {
            video_src->stop_capture();
        }
    }

    m_scene_list.erase(m_scene_list.begin() + index);

    if (m_scene_list.empty()) {
        add_scene("场景 1");
        m_current_scene_index = 0;
    } else if (index <= m_current_scene_index) {
        m_current_scene_index = std::max(0, m_current_scene_index - 1);
    }
    m_current_scene_index = std::min(m_current_scene_index,
                                     static_cast<int>(m_scene_list.size()) - 1);
}

void ScenePreviewWidget::switch_to_scene(int index) {
    if (index < 0 || index >= static_cast<int>(m_scene_list.size())) return;
    if (index == m_current_scene_index) return;

    for (auto &src : current_storage()) {
        auto *video_src = dynamic_cast<VideoSource *>(src.get());
        if (video_src) video_src->pause_capture();
    }

    m_current_scene_index = index;

    for (auto &src : current_storage()) {
        auto *video_src = dynamic_cast<VideoSource *>(src.get());
        if (video_src) video_src->resume_capture();
    }

    current_scene().clear_selection();
}

void ScenePreviewWidget::pause_all_non_current_scenes() {
    for (int i = 0; i < static_cast<int>(m_scene_list.size()); ++i) {
        if (i == m_current_scene_index) continue;
        for (auto &src : m_scene_list[i].source_storage) {
            auto *video_src = dynamic_cast<VideoSource *>(src.get());
            if (video_src) video_src->pause_capture();
        }
    }
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

    const int padding = 6;
    m_viewX += padding;
    m_viewY += padding;
    m_viewW -= 2 * padding;
    m_viewH -= 2 * padding;

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
    Source *selected = current_scene().selected_source();
    if (!selected || !selected->visible) return;

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // Step 3a: set stencil value to 1 for selected source area
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

    // Step 3b: restore stencil value to 2 for canvas area
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

    for (Source *source: current_scene().get_sources()) {
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

    current_scene().render_selection_box();
}

void ScenePreviewWidget::render_mosaic_for_selected() {
    if (!current_scene().selected_source()) return;

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
    for (Source *src: current_scene().get_sources()) {
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
    Qt::CursorShape shape = current_scene().get_cursor_for_current_pos();
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
    for (auto &sd : m_scene_list) {
        for (Source *src : sd.scene.get_sources()) {
            src->load_resources();
        }
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

    if (m_frame_capture_callback) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_last_capture_time).count();
        if (elapsed >= 1000 / m_capture_fps) {
            m_last_capture_time = now;
            int w = m_viewW;
            int h = m_viewH;
            if (w <= 0 || h <= 0) return;
            int stride = w * 4;
            std::vector<uint8_t> pixels(stride * h);
            glReadPixels(m_viewX, m_viewY, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
            // OpenGL reads image upside down, flip vertically
            std::vector<uint8_t> flipped(stride * h);
            for (int y = 0; y < h; ++y) {
                memcpy(flipped.data() + y * stride,
                       pixels.data() + (h - 1 - y) * stride,
                       stride);
            }
            m_frame_capture_callback(flipped.data(), stride, w, h);
        }
    }
}

void ScenePreviewWidget::mousePressEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    Source *old_sel = current_scene().selected_source();
    current_scene().on_mouse_press(canvas_pos);
    update_cursor();
    Source *new_sel = current_scene().selected_source();
    if (old_sel != new_sel) {
        int idx = -1;
        if (new_sel) {
            auto &storage = current_storage();
            for (size_t i = 0; i < storage.size(); i++) {
                if (storage[i].get() == new_sel) {
                    idx = static_cast<int>(i);
                    break;
                }
            }
        }
        emit canvas_selection_changed(idx);
    }
}

void ScenePreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    QPointF canvas_pos = screen_to_canvas(event->position());
    current_scene().on_mouse_move(canvas_pos);
    update_cursor();
}

void ScenePreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    InteractionMode prev_mode = current_scene().interaction_mode();
    current_scene().on_mouse_release();
    update_cursor();
    if (prev_mode == InteractionMode::Dragging ||
        prev_mode == InteractionMode::Resizing) {
        emit source_position_changed();
    }
}
