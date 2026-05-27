#ifndef OBSIM_SETTINGSPREVIEWWIDGET_H
#define OBSIM_SETTINGSPREVIEWWIDGET_H

#include "../base/previewbasewidget.h"
#include <QLabel>
#include <QVBoxLayout>

class SettingsPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    explicit SettingsPreviewWidget(QWidget *parent = nullptr);

protected:
    QWidget* create_control_area() override;

private:
    QListWidget *m_param_list = nullptr;
    QWidget *m_param_right = nullptr;
};

#endif