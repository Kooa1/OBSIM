//
// Created by 66 on 2026/4/25.
//

#include "controlbar.h"

ControlBar::ControlBar(QWidget *parent) : QWidget(parent) {
    this->setMinimumHeight(220);

    init_UI();
}

void ControlBar::init_UI() {
    main_layout = new QHBoxLayout(this);
    main_splitter = new QSplitter(Qt::Horizontal, this);
}

void ControlBar::init_control_block() {
}

void ControlBar::init_layout() {
    main_splitter->addWidget(main_splitter);
}
