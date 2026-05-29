#include "previewbasewidget.h"
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

    m_owner->on_frame_update();
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

void PreviewBaseWidget::on_frame_update() {
    if (m_source) m_source->update_frame();
}

PreviewBaseWidget::SplitControlPanel
PreviewBaseWidget::create_split_panel(const QString &title, QWidget *parent) {
    SplitControlPanel result;

    auto *h_splitter = new QSplitter(Qt::Horizontal, parent);
    h_splitter->setChildrenCollapsible(false);
    result.splitter = h_splitter;

    auto *left_widget = new QWidget(h_splitter);
    auto *left_layout = new QVBoxLayout(left_widget);
    left_layout->setContentsMargins(4, 4, 4, 4);
    left_layout->setSpacing(4);

    auto *title_bar = new QHBoxLayout();
    auto *title_label = new QLabel(title, left_widget);
    title_label->setObjectName("block_title");
    title_bar->addWidget(title_label);
    title_bar->addStretch();
    left_layout->addLayout(title_bar);

    result.list_widget = new QListWidget(left_widget);
    result.list_widget->setAlternatingRowColors(true);
    left_layout->addWidget(result.list_widget);

    auto *btn_layout = new QHBoxLayout();
    result.btn_add = new QPushButton("＋", left_widget);
    result.btn_remove = new QPushButton("－", left_widget);
    result.btn_add->setFixedSize(28, 28);
    result.btn_remove->setFixedSize(28, 28);
    btn_layout->addStretch();
    btn_layout->addWidget(result.btn_add);
    btn_layout->addWidget(result.btn_remove);
    left_layout->addLayout(btn_layout);

    h_splitter->addWidget(left_widget);

    result.right_panel = new QWidget(h_splitter);
    // result.right_panel->setStyleSheet("background: #2a2a2a;");
    h_splitter->addWidget(result.right_panel);

    h_splitter->setStretchFactor(0, 0);
    h_splitter->setStretchFactor(1, 1);
    h_splitter->setSizes({200, 400});

    return result;
}

void PreviewBaseWidget::closeEvent(QCloseEvent *event) {
    stop_preview();
    emit close_requested();
    event->accept();
}
