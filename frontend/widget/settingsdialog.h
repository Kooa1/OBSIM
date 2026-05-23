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

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString record_output_path() const;

private:
    void init_ui();
    void init_output_page();

    QListWidget *m_page_list;
    QStackedWidget *m_page_stack;

    QLineEdit *m_output_path_edit;
};

#endif
