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
    if (m_filter) {
        m_pending_params = m_filter->params();
    } else {
        m_pending_params = FilterParams{};
    }
}

void FilterPreviewWidget::showEvent(QShowEvent *event) {
    PreviewBaseWidget::showEvent(event);
    if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
        vs->start_filter();
    }
}

void FilterPreviewWidget::hideEvent(QHideEvent *event) {
    if (!m_applying) {
        if (auto *vs = dynamic_cast<VideoSource*>(m_source)) {
            vs->stop_filter();
        }
    }
    m_applying = false;
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

    m_param_stack = new QStackedWidget(panel.right_panel);
    auto *right_layout = new QVBoxLayout(panel.right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->addWidget(m_param_stack);

    auto *btn_layout = new QHBoxLayout();
    btn_layout->addStretch();
    auto *reset_btn = new QPushButton("重置参数");
    btn_layout->addWidget(reset_btn);
    auto *cancel_btn = new QPushButton("取消");
    btn_layout->addWidget(cancel_btn);
    auto *apply_btn = new QPushButton("确定");
    apply_btn->setDefault(true);
    btn_layout->addWidget(apply_btn);
    right_layout->addLayout(btn_layout);

    connect(reset_btn, &QPushButton::clicked, this, &FilterPreviewWidget::on_reset);
    connect(cancel_btn, &QPushButton::clicked, this, [this]() {
        close();
    });
    connect(apply_btn, &QPushButton::clicked, this, &FilterPreviewWidget::on_apply);

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
}

void FilterPreviewWidget::on_item_clicked(int currentRow) {
    if (currentRow < 0) return;
    build_param_pages();
    if (currentRow < m_param_stack->count()) {
        m_param_stack->setCurrentIndex(currentRow);
    }
}

void FilterPreviewWidget::inject_current_params() {
    if (m_filter) {
        m_filter->set_params(m_pending_params);
    }
}

void FilterPreviewWidget::apply_flip_code(int code) {
    m_pending_params.flip_code = code;
    if (m_pending_params.enable_flip) inject_current_params();
}

void FilterPreviewWidget::apply_brightness(int value) {
    m_pending_params.brightness = value / 100.0f;
    if (m_pending_params.enable_color_adjust) inject_current_params();
}

void FilterPreviewWidget::apply_contrast(int value) {
    m_pending_params.contrast = value / 100.0f;
    if (m_pending_params.enable_color_adjust) inject_current_params();
}

void FilterPreviewWidget::apply_saturation(int value) {
    m_pending_params.saturation = value / 100.0f;
    if (m_pending_params.enable_color_adjust) inject_current_params();
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

    checkbox->setChecked(m_pending_params.enable_flip);
    combo->setCurrentIndex(combo->findData(m_pending_params.flip_code));

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_pending_params.enable_flip = checked;
        inject_current_params();
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
    layout->addWidget(desc);

    checkbox->setChecked(m_pending_params.enable_grayscale);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_pending_params.enable_grayscale = checked;
        inject_current_params();
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
    bright_slider->setValue(static_cast<int>(m_pending_params.brightness * 100.0f));
    layout->addWidget(new QLabel("亮度"));
    layout->addWidget(bright_slider);

    auto *contrast_slider = new QSlider(Qt::Horizontal);
    contrast_slider->setRange(0, 300);
    contrast_slider->setValue(static_cast<int>(m_pending_params.contrast * 100.0f));
    layout->addWidget(new QLabel("对比度"));
    layout->addWidget(contrast_slider);

    auto *sat_slider = new QSlider(Qt::Horizontal);
    sat_slider->setRange(0, 300);
    sat_slider->setValue(static_cast<int>(m_pending_params.saturation * 100.0f));
    layout->addWidget(new QLabel("饱和度"));
    layout->addWidget(sat_slider);

    checkbox->setChecked(m_pending_params.enable_color_adjust);

    connect(checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_pending_params.enable_color_adjust = checked;
        inject_current_params();
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

QWidget* FilterPreviewWidget::create_no_param_page(const char *text) {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addStretch();
    return page;
}

void FilterPreviewWidget::on_reset() {
    m_pending_params = FilterParams{};
    inject_current_params();
    while (m_param_stack->count() > 0) {
        QWidget *page = m_param_stack->widget(0);
        m_param_stack->removeWidget(page);
        delete page;
    }
}

void FilterPreviewWidget::on_apply() {
    inject_current_params();
    m_applying = true;
    emit apply_confirmed();
}