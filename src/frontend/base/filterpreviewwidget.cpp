#include "filterpreviewwidget.h"
#include <QLabel>
#include <QVBoxLayout>

FilterPreviewWidget::FilterPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("滤镜");
    m_control_area = create_control_area();
    add_control_area();
}

QWidget* FilterPreviewWidget::create_control_area() {
    auto *widget = new QWidget(this);
    widget->setMinimumHeight(60);
    widget->setStyleSheet("background: #333333;");
    auto *layout = new QVBoxLayout(widget);
    auto *label = new QLabel("滤镜控制区（待实现）", widget);
    label->setStyleSheet("color: #999999; font-size: 12px; background: transparent;");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    return widget;
}