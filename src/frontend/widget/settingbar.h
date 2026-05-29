#ifndef OBSIM_SETTINGBAR_H
#define OBSIM_SETTINGBAR_H

#include "../../utils/PCH.h"
#include "../../core/base/source.h"

/// @brief Toolbar for displaying selected source info and accessing filter/settings
class SettingBar : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SettingBar(QWidget *parent = nullptr);
    ~SettingBar() override;

    /// @brief Set the text shown for current selection
    /// @param text Selection label text
    void set_selection_text(const QString &text);
    /// @brief Set the currently selected source
    /// @param source Pointer to source
    void set_current_source(Source *source);

signals:
    /// @brief Emitted when filter button is clicked
    /// @param source Current source
    void filter_clicked(Source *source);
    /// @brief Emitted when settings button is clicked
    /// @param source Current source
    void settings_clicked(Source *source);

private:
    void init_UI();

    QLabel *m_source_label = nullptr;       ///< Source name label
    QPushButton *m_filter_btn = nullptr;     ///< Filter button
    QPushButton *m_settings_btn = nullptr;   ///< Settings button
    Source *m_current_source = nullptr;      ///< Currently selected source
};

#endif