#ifndef OBSIM_SETTINGSDIALOG_H
#define OBSIM_SETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSplitter>
#include <QFutureWatcher>
#include <QPointer>
#include <QDesktopServices>

#include "../../utils/configmanager.h"

/// @brief Settings dialog for output path and streaming URL configuration
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    /// @param initial_page Index of the initial page to show
    explicit SettingsDialog(QWidget *parent = nullptr, int initial_page = 0);

    /// @brief Get the configured recording output path
    /// @return Output path string
    QString record_output_path() const;
    /// @brief Get the configured streaming URL
    /// @return Streaming URL string
    QString streaming_url() const;

private:
    void init_ui();
    void init_output_page();
    void init_streaming_page();

    QListWidget *m_page_list;           ///< Left page list
    QStackedWidget *m_page_stack;       ///< Right page stack
    QLineEdit *m_output_path_edit;      ///< Output path editor
    QLineEdit *m_stream_url_edit;       ///< Streaming URL editor
};

#endif