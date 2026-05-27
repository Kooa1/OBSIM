#include "../base/filterpreviewwidget.h"

FilterPreviewWidget::FilterPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("滤镜");
    m_control_area = create_control_area();
    add_control_area();
}

QWidget* FilterPreviewWidget::create_control_area() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto panel = create_split_panel("滤镜参数", container);

    panel.list_widget->addItem("翻转");
    panel.list_widget->addItem("灰度");
    panel.list_widget->addItem("高斯模糊");
    panel.list_widget->addItem("锐化");
    panel.list_widget->addItem("边缘检测");
    panel.list_widget->addItem("颜色调整");
    panel.list_widget->addItem("裁剪");

    layout->addWidget(panel.splitter);

    m_param_list = panel.list_widget;
    m_param_right = panel.right_panel;

    return container;
}