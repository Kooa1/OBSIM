#ifndef OBSIM_SETTINGBAR_H
#define OBSIM_SETTINGBAR_H

#include "../utils/PCH.h"

class SettingBar : public QWidget {
    Q_OBJECT

public:
    explicit SettingBar(QWidget *parent = nullptr);

    ~SettingBar() override;

    void set_selection_text(const QString &text);

private:
    void init_UI();

    void init_CSS();

    QLabel *m_source_label = nullptr;
    QPushButton *m_filter_btn = nullptr;
    QPushButton *m_settings_btn = nullptr;
};


#endif //OBSIM_SETTINGBAR_H