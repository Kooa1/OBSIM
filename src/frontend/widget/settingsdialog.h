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

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr, int initial_page = 0);

    QString record_output_path() const;
    QString streaming_url() const;

private:
    void init_ui();
    void init_output_page();
    void init_streaming_page();

    QListWidget *m_page_list;
    QStackedWidget *m_page_stack;

    QLineEdit *m_output_path_edit;
    QLineEdit *m_stream_url_edit;
};

#endif
