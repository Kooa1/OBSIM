#ifndef OBSIM_SETTINGBAR_H
#define OBSIM_SETTINGBAR_H

#include "../../utils/PCH.h"
#include "../../core/base/source.h"

class SettingBar : public QWidget {
    Q_OBJECT

public:
    explicit SettingBar(QWidget *parent = nullptr);
    ~SettingBar() override;

    void set_selection_text(const QString &text);
    void set_current_source(Source *source);

signals:
    void filter_clicked(Source *source);
    void settings_clicked(Source *source);

private:
    void init_UI();

    QLabel *m_source_label = nullptr;
    QPushButton *m_filter_btn = nullptr;
    QPushButton *m_settings_btn = nullptr;
    Source *m_current_source = nullptr;
};

#endif