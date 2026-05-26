#include "settingspreviewwidget.h"

SettingsPreviewWidget::SettingsPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("设置");
    m_control_area = create_control_area();
    add_control_area();
}

QWidget* SettingsPreviewWidget::create_control_area() {
    auto *widget = new QWidget(this);
    widget->setMinimumHeight(60);
    widget->setStyleSheet("background: #333333;");
    auto *layout = new QVBoxLayout(widget);
    auto *label = new QLabel("源设置控制区（待实现）", widget);
    label->setStyleSheet("color: #999999; font-size: 12px; background: transparent;");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    return widget;
}