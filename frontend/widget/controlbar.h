//
// Created by 66 on 2026/4/25.
//

#ifndef OBSIM_CONTROLBAR_H
#define OBSIM_CONTROLBAR_H

#include "../utils/PCH.h"

class ControlBar : public QWidget {
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);

    ~ControlBar() override = default;

private:
    void init_UI();

    void init_control_block();

    void init_layout();

private:
    QHBoxLayout *main_layout = nullptr;

    QSplitter *main_splitter = nullptr;
};


#endif //OBSIM_CONTROLBAR_H
