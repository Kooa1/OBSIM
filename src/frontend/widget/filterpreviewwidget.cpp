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

    auto add_checkable = [&](const QString &text) {
        auto *item = new QListWidgetItem(text);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        panel.list_widget->addItem(item);
    };
    add_checkable("翻转");
    add_checkable("灰度");
    add_checkable("高斯模糊");
    add_checkable("锐化");
    add_checkable("边缘检测");
    add_checkable("颜色调整");
    add_checkable("裁剪");

    m_param_stack = new QStackedWidget(panel.right_panel);
    auto *right_layout = new QVBoxLayout(panel.right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->addWidget(m_param_stack);

    layout->addWidget(panel.splitter);

    m_param_list = panel.list_widget;

    connect(m_param_list, &QListWidget::currentRowChanged,
            this, &FilterPreviewWidget::on_item_clicked);
    connect(m_param_list, &QListWidget::itemChanged,
            this, [this](QListWidgetItem *item) {
        int row = m_param_list->row(item);
        if (row < 0 || !m_filter) return;
        FilterParams p = m_filter->params();
        auto checked = (item->checkState() == Qt::Checked);
        switch (row) {
            case 0: p.enable_flip = checked; break;
            case 1: p.enable_grayscale = checked; break;
            case 2: p.enable_gaussian_blur = checked; break;
            case 3: p.enable_sharpen = checked; break;
            case 4: p.enable_edge_detect = checked; break;
            case 5: p.enable_color_adjust = checked; break;
            case 6: p.enable_crop = checked; break;
        }
        m_filter->set_params(p);
    });

    return container;
}

void FilterPreviewWidget::build_param_pages() {
    if (!m_filter || m_param_stack->count() > 0) return;

    m_param_stack->addWidget(create_flip_page());
    m_param_stack->addWidget(create_no_param_page("灰度：将图像转换为灰度"));
    m_param_stack->addWidget(create_blur_page());
    m_param_stack->addWidget(create_no_param_page("锐化：增强图像边缘"));
    m_param_stack->addWidget(create_no_param_page("边缘检测：Canny 算法"));
    m_param_stack->addWidget(create_color_adjust_page());
    m_param_stack->addWidget(create_no_param_page("裁剪：设置裁剪区域"));
}

void FilterPreviewWidget::on_item_clicked(int currentRow) {
    if (currentRow < 0 || !m_filter) return;

    build_param_pages();

    if (currentRow < m_param_stack->count()) {
        m_param_stack->setCurrentIndex(currentRow);
    }

    FilterParams p = m_filter->params();
    bool checked = false;
    switch (currentRow) {
        case 0: checked = p.enable_flip; break;
        case 1: checked = p.enable_grayscale; break;
        case 2: checked = p.enable_gaussian_blur; break;
        case 3: checked = p.enable_sharpen; break;
        case 4: checked = p.enable_edge_detect; break;
        case 5: checked = p.enable_color_adjust; break;
        case 6: checked = p.enable_crop; break;
    }
    auto *item = m_param_list->item(currentRow);
    if (item) item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void FilterPreviewWidget::apply_flip_code(int code) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.flip_code = code;
    p.enable_flip = true;
    m_filter->set_params(p);
}

void FilterPreviewWidget::apply_blur_ksize(int ksize) {
    if (!m_filter) return;
    FilterParams p = m_filter->params();
    p.blur_ksize = ksize;
    p.enable_gaussian_blur = true;
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

QWidget* FilterPreviewWidget::create_flip_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel("翻转方向");
    auto *combo = new QComboBox();
    combo->addItem("水平翻转", 1);
    combo->addItem("垂直翻转", 0);
    combo->addItem("双向翻转", -1);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo](int) {
        apply_flip_code(combo->currentData().toInt());
    });

    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_blur_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel("模糊半径");
    auto *spin = new QSpinBox();
    spin->setRange(1, 31);
    spin->setSingleStep(2);
    spin->setValue(5);

    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FilterPreviewWidget::apply_blur_ksize);

    layout->addWidget(label);
    layout->addWidget(spin);
    layout->addStretch();
    return page;
}

QWidget* FilterPreviewWidget::create_color_adjust_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *bright_label = new QLabel("亮度");
    auto *bright_slider = new QSlider(Qt::Horizontal);
    bright_slider->setRange(-100, 100);
    bright_slider->setValue(0);
    connect(bright_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_brightness);

    auto *contrast_label = new QLabel("对比度");
    auto *contrast_slider = new QSlider(Qt::Horizontal);
    contrast_slider->setRange(0, 300);
    contrast_slider->setValue(100);
    connect(contrast_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_contrast);

    auto *sat_label = new QLabel("饱和度");
    auto *sat_slider = new QSlider(Qt::Horizontal);
    sat_slider->setRange(0, 300);
    sat_slider->setValue(100);
    connect(sat_slider, &QSlider::valueChanged,
            this, &FilterPreviewWidget::apply_saturation);

    layout->addWidget(bright_label);
    layout->addWidget(bright_slider);
    layout->addWidget(contrast_label);
    layout->addWidget(contrast_slider);
    layout->addWidget(sat_label);
    layout->addWidget(sat_slider);
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