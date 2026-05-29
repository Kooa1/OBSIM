#ifndef OBSIM_SETTINGSPREVIEWWIDGET_H
#define OBSIM_SETTINGSPREVIEWWIDGET_H

#include "../base/previewbasewidget.h"
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QVector>

/// @brief Preview widget for viewing and editing source settings
class SettingsPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SettingsPreviewWidget(QWidget *parent = nullptr);

    /// @brief Set all available sources
    /// @param sources List of source pointers
    void set_all_sources(const QVector<Source*> &sources);
    /// @brief Populate the source combo box
    void populate_source_list();

signals:
    /// @brief Emitted when a source name is changed
    /// @param src Source pointer
    /// @param new_name New name string
    void source_name_changed(Source *src, const QString &new_name);

protected:
    QWidget* create_control_area() override;

private:
    QComboBox *m_source_combo = nullptr; ///< Source selection combo
    QLineEdit *m_name_edit = nullptr;    ///< Name editor
    QVector<Source*> m_all_sources;      ///< All available sources
    QString m_original_name;             ///< Original source name

private slots:
    void on_apply();
    void on_cancel();
};

#endif