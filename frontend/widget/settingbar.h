//
// Created by 66 on 2026/4/27.
//

#ifndef OBSIM_SETTINGBAR_H
#define OBSIM_SETTINGBAR_H

#include "../utils/PCH.h"

class SettingBar : public QWidget {
    Q_OBJECT

public:
    explicit SettingBar(QWidget *parent = nullptr);

    ~SettingBar() override;

private:
    void init_UI();

    void init_CSS();
};


#endif //OBSIM_SETTINGBAR_H
