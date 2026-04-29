//
// Created by 66 on 2026/4/27.
//

#include "settingbar.h"

SettingBar::SettingBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    init_UI();
}

SettingBar::~SettingBar() = default;

void SettingBar::init_UI() {
    init_CSS();
}

void SettingBar::init_CSS() {
    this->setFixedHeight(50);
    this->setStyleSheet("QWidget "
        "{ background : #808080;"
        " border-radius: 10px; }");
}
