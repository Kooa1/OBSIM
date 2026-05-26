#include "previewbasewidget.h"
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include "../../core/base/source.h"

// ===== PreviewGLWidget =====

PreviewBaseWidget::PreviewGLWidget::PreviewGLWidget(PreviewBaseWidget *owner, QWidget *parent)
    : QOpenGLWidget(parent), m_owner(owner) {
}

void PreviewBaseWidget::PreviewGLWidget::initializeGL() {
    QOpenGLWidget::initializeGL();
}

void PreviewBaseWidget::PreviewGLWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void PreviewBaseWidget::PreviewGLWidget::paintGL() {
    if (!m_owner || !m_owner->m_source) return;

    const int w = width() * devicePixelRatio();
    const int h = height() * devicePixelRatio();

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Source *src = m_owner->m_source;
    float srcW = src->base_width > 0 ? src->base_width : 1.0f;
    float srcH = src->base_height > 0 ? src->base_height : 1.0f;
    float scaleX = static_cast<float>(w) / srcW;
    float scaleY = static_cast<float>(h) / srcH;
    float scale = qMin(scaleX, scaleY);
    float viewW = srcW * scale;
    float viewH = srcH * scale;
    float viewX = (w - viewW) / 2.0f;
    float viewY = (h - viewH) / 2.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(viewX, viewY, 0);
    glScalef(viewW / srcW, viewH / srcH, 1);

    src->update_frame();
    src->render();
}

// ===== PreviewBaseWidget =====

PreviewBaseWidget::PreviewBaseWidget(QWidget *parent)
    : QWidget(parent) {
    setWindowTitle("预览");
    setWindowFlags(Qt::Window);
    setMinimumSize(800, 600);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setChildrenCollapsible(false);

    m_gl_widget = new PreviewGLWidget(this, m_splitter);
    m_gl_widget->setMinimumHeight(240);
    m_splitter->addWidget(m_gl_widget);

    layout->addWidget(m_splitter);

    m_render_timer = new QTimer(this);
    connect(m_render_timer, &QTimer::timeout, this, [this]() {
        if (m_gl_widget) m_gl_widget->update();
    });

    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        double occupancy = 0.55;
        int targetH = static_cast<int>(available.height() * occupancy);
        int targetW = static_cast<int>(targetH * 16.0 / 9.0);
        if (targetW > available.width() * 0.9) {
            targetW = static_cast<int>(available.width() * 0.9);
            targetH = static_cast<int>(targetW * 9.0 / 16.0);
        }
        resize(targetW, targetH);
    } else {
        resize(960, 640);
    }
}

PreviewBaseWidget::~PreviewBaseWidget() {
    stop_preview();
}

void PreviewBaseWidget::set_source(Source *source) {
    m_source = source;
    if (source) {
        setWindowTitle(source->display_name);
    }
    if (m_gl_widget) m_gl_widget->update();
}

void PreviewBaseWidget::start_preview() {
    m_render_timer->start(33);
}

void PreviewBaseWidget::stop_preview() {
    m_render_timer->stop();
}

void PreviewBaseWidget::add_control_area() {
    if (!m_control_area || !m_splitter) return;
    m_splitter->addWidget(m_control_area);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
}

void PreviewBaseWidget::closeEvent(QCloseEvent *event) {
    stop_preview();
    emit close_requested();
    event->accept();
}
