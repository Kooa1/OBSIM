#ifndef OBSIM_SETTINGSPREVIEWWIDGET_H
#define OBSIM_SETTINGSPREVIEWWIDGET_H

#include "../base/previewbasewidget.h"
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QVector>

class SettingsPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    explicit SettingsPreviewWidget(QWidget *parent = nullptr);

    void set_all_sources(const QVector<Source*> &sources);
    void populate_source_list();

signals:
    void source_name_changed(Source *src, const QString &new_name);

protected:
    QWidget* create_control_area() override;

private:
    QComboBox *m_source_combo = nullptr;
    QLineEdit *m_name_edit = nullptr;
    QVector<Source*> m_all_sources;
};

#endif