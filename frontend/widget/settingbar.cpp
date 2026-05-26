#include "settingbar.h"

SettingBar::SettingBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    init_UI();
}

SettingBar::~SettingBar() = default;

void SettingBar::set_selection_text(const QString &text) {
    m_source_label->setText(text);
    bool has_selection = (text != "未选择输入源");
    m_filter_btn->setEnabled(has_selection);
    m_settings_btn->setEnabled(has_selection);
}

void SettingBar::init_UI() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);

    m_source_label = new QLabel("未选择输入源", this);
    m_filter_btn = new QPushButton("滤镜", this);
    m_settings_btn = new QPushButton("设置", this);

    m_filter_btn->setFixedSize(60, 30);
    m_settings_btn->setFixedSize(60, 30);

    layout->addWidget(m_source_label);
    layout->addStretch();
    layout->addWidget(m_filter_btn);
    layout->addWidget(m_settings_btn);

    init_CSS();
}

void SettingBar::init_CSS() {
    this->setFixedHeight(50);
    this->setStyleSheet(
        "SettingBar {"
        "  background: #808080;"
        "  border-radius: 10px;"
        "}"
        "QLabel {"
        "  color: white;"
        "  font-size: 14px;"
        "  background: transparent;"
        "}"
        "QPushButton {"
        "  color: white;"
        "  background: #555555;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #666666;"
        "}"
        "QPushButton:disabled {"
        "  color: #999999;"
        "  background: #444444;"
        "}"
    );
}