#include "filterpreviewwidget.h"
#include "../../core/filter/opencvfilter.h"
#include "../../core/videosource.h"

FilterPreviewWidget::FilterPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("滤镜");
    m_control_area = create_control_area();
    add_control_area();
}

void FilterPreviewWidget::set_source(Source *source) {
    PreviewBaseWidget::set_source(source);
    init_filter();
}

void FilterPreviewWidget::init_filter() {
    m_filter = nullptr;
    if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
        m_filter = vs->filter();
    }
}

void FilterPreviewWidget::showEvent(QShowEvent *event) {
    PreviewBaseWidget::showEvent(event);
    if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
        vs->start_filter();
    }
}

void FilterPreviewWidget::hideEvent(QHideEvent *event) {
    if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
        vs->stop_filter();
    }
    PreviewBaseWidget::hideEvent(event);
}

void FilterPreviewWidget::on_frame_update() {
    if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
        vs->update_filtered_frame();
    } else if (m_source) {
        m_source->update_frame();
    }
}

QWidget* FilterPreviewWidget::create_control_area() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto panel = create_split_panel("滤镜参数", container);

    auto add_item = [&](const QString &text) {
        auto *item = new QListWidgetItem(text);
        panel.list_widget->addItem(item);
    };
    add_item("翻转");
    add_item("灰度");
    add_item("颜色调整");
    add_item("裁剪");

    m_param_stack = new QStackedWidget(panel.right_panel);
    m_param_stack->setStyleSheet("background: #2a2a2a;");
    auto *right_layout = new QVBoxLayout(panel.right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->addWidget(m_param_stack);

    auto *reset_btn = new QPushButton("重置参数");
    right_layout->addWidget(reset_btn);
    connect(reset_btn, &QPushButton::clicked, this, &FilterPreviewWidget::on_reset);

    layout->addWidget(panel.splitter);

    m_param_list = panel.list_widget;

    connect(m_param_list, &QListWidget::currentRowChanged,
            this, &FilterPreviewWidget::on_item_clicked);

    return container;
}

void FilterPreviewWidget::build_param_pages() {
    if (!m_filter || m_param_stack->count() > 0) return;

    m_param_stack->addWidget(create_flip_page());
    m_param_stack->addWidget(create_grayscale_page());
    m_param_stack->addWidget(create_color_adjust_page());
    m_param_stack->addWidget(create_crop_page());
}

void FilterPreviewWidget::on_item_clicked(int currentRow) {
    if (currentRow < 0) return;
    build_param_pages();
    if (currentRow < m_param_stack->count()) {
        m_param_stack->setCurrentIndex(currentRow);
    }
}

void FilterPreviewWidget::apply_flip_code(int code) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.flip_code = code;
    p.enable_flip = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_brightness(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.brightness = value / 100.0f;
    p.enable_color_adjust = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_contrast(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.contrast = value / 100.0f;
    p.enable_color_adjust = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_saturation(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.saturation = value / 100.0f;
    p.enable_color_adjust = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_crop_x(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.crop_rect.x = value;
    p.enable_crop = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_crop_y(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.crop_rect.y = value;
    p.enable_crop = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_crop_w(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.crop_rect.width = value;
    p.enable_crop = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_crop_h(int value) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.crop_rect.height = value;
    p.enable_crop = true;
    m_filter->set_params(p);
}

QWidget* FilterPreviewWidget::create_flip_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *header = new QHBoxLayout();
    auto *checkbox = new QCheckBox("启用翻转");
    header->addWidget(checkbox);
    header->addStretch();
    layout->addLayout(header);

    auto *combo = new QComboBox();
    combo->addItem("水平翻转", 1);
    combo->addItem("垂直翻转", 0);
    combo->addItem("双向翻转", -1);
    layout->addWidget(new QLabel("翻转方向"));
    layout->addWidget(combo);

    FilterParams p = m_filter->params();
    checkbox->setChecked(p.enable_flip);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_filter) return;
        FilterParams p = m_filter->params();
        p.enable_flip = checked;
        m_filter->set_params(p);
    });
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo](int) {
        apply_flip_code(combo->currentData().toInt());
    });

    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_grayscale_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *header = new QHBoxLayout();
    auto *checkbox = new QCheckBox("启用灰度");
    header->addWidget(checkbox);
    header->addStretch();
    layout->addLayout(header);

    auto *desc = new QLabel("将图像转换为灰度");
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #aaa;");
    layout->addWidget(desc);

    FilterParams p = m_filter->params();
    checkbox->setChecked(p.enable_grayscale);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_filter) return;
        FilterParams p = m_filter->params();
        p.enable_grayscale = checked;
        m_filter->set_params(p);
    });

    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_color_adjust_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *header = new QHBoxLayout();
    auto *checkbox = new QCheckBox("启用颜色调整");
    header->addWidget(checkbox);
    header->addStretch();
    layout->addLayout(header);

    auto *bright_slider = new QSlider(Qt::Horizontal);
    bright_slider->setRange(-100, 100);
    bright_slider->setValue(0);
    layout->addWidget(new QLabel("亮度"));
    layout->addWidget(bright_slider);

    auto *contrast_slider = new QSlider(Qt::Horizontal);
    contrast_slider->setRange(0, 300);
    contrast_slider->setValue(100);
    layout->addWidget(new QLabel("对比度"));
    layout->addWidget(contrast_slider);

    auto *sat_slider = new QSlider(Qt::Horizontal);
    sat_slider->setRange(0, 300);
    sat_slider->setValue(100);
    layout->addWidget(new QLabel("饱和度"));
    layout->addWidget(sat_slider);

    FilterParams p = m_filter->params();
    checkbox->setChecked(p.enable_color_adjust);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_filter) return;
        FilterParams p = m_filter->params();
        p.enable_color_adjust = checked;
        m_filter->set_params(p);
    });
    connect(bright_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_brightness);
    connect(contrast_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_contrast);
    connect(sat_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_saturation);

    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_crop_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *header = new QHBoxLayout();
    auto *checkbox = new QCheckBox("启用裁剪");
    header->addWidget(checkbox);
    header->addStretch();
    layout->addLayout(header);

    auto *x_spin = new QSpinBox();
    x_spin->setRange(0, 9999);
    layout->addWidget(new QLabel("X"));
    layout->addWidget(x_spin);

    auto *y_spin = new QSpinBox();
    y_spin->setRange(0, 9999);
    layout->addWidget(new QLabel("Y"));
    layout->addWidget(y_spin);

    auto *w_spin = new QSpinBox();
    w_spin->setRange(1, 9999);
    w_spin->setValue(640);
    layout->addWidget(new QLabel("宽度"));
    layout->addWidget(w_spin);

    auto *h_spin = new QSpinBox();
    h_spin->setRange(1, 9999);
    h_spin->setValue(480);
    layout->addWidget(new QLabel("高度"));
    layout->addWidget(h_spin);

    FilterParams p = m_filter->params();
    checkbox->setChecked(p.enable_crop);
    x_spin->setValue(p.crop_rect.x);
    y_spin->setValue(p.crop_rect.y);
    if (p.crop_rect.width > 0) w_spin->setValue(p.crop_rect.width);
    if (p.crop_rect.height > 0) h_spin->setValue(p.crop_rect.height);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_filter) return;
        FilterParams p = m_filter->params();
        p.enable_crop = checked;
        m_filter->set_params(p);
    });
    connect(x_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FilterPreviewWidget::apply_crop_x);
    connect(y_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FilterPreviewWidget::apply_crop_y);
    connect(w_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FilterPreviewWidget::apply_crop_w);
    connect(h_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FilterPreviewWidget::apply_crop_h);

    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_no_param_page(const char *text) {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setStyleSheet("color: #aaa;");
    layout->addWidget(label);
    layout->addStretch();
    return page;
}

void FilterPreviewWidget::on_reset() {
    if (!m_filter) return;
    m_filter->set_params(FilterParams{});
    while (m_param_stack->count() > 0) {
        QWidget *page = m_param_stack->widget(0);
        m_param_stack->removeWidget(page);
        delete page;
    }
}