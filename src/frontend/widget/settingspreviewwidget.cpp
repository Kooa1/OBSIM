#include "settingspreviewwidget.h"

SettingsPreviewWidget::SettingsPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("设置");
    m_control_area = create_control_area();
    add_control_area();
}

QWidget* SettingsPreviewWidget::create_control_area() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto panel = create_split_panel("源属性", container);

    panel.list_widget->addItem("位置");
    panel.list_widget->addItem("缩放");
    panel.list_widget->addItem("旋转");
    panel.list_widget->addItem("裁剪");
    panel.list_widget->addItem("不透明度");

    layout->addWidget(panel.splitter);

    m_param_list = panel.list_widget;
    m_param_right = panel.right_panel;

    return container;
}