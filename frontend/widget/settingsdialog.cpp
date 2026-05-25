#include "settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent, int initial_page)
    : QDialog(parent) {
    setWindowTitle("设置");
    setMinimumSize(560, 360);
    init_ui();
    m_page_list->setCurrentRow(initial_page);
}

QString SettingsDialog::record_output_path() const {
    return m_output_path_edit->text();
}

void SettingsDialog::init_ui() {
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧页面列表
    m_page_list = new QListWidget();
    m_page_list->setFixedWidth(120);
    m_page_list->addItem("输出");
    m_page_list->addItem("直播");
    m_page_list->setCurrentRow(0);

    // 右侧页面栈
    m_page_stack = new QStackedWidget();
    init_output_page();
    init_streaming_page();

    splitter->addWidget(m_page_list);
    splitter->addWidget(m_page_stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    main_layout->addWidget(splitter);

    connect(m_page_list, &QListWidget::currentRowChanged, m_page_stack, &QStackedWidget::setCurrentIndex);

    // 底部按钮
    auto *btn_layout = new QHBoxLayout();
    btn_layout->addStretch();
    auto *ok_btn = new QPushButton("确定");
    auto *cancel_btn = new QPushButton("取消");
    btn_layout->addWidget(ok_btn);
    btn_layout->addWidget(cancel_btn);
    main_layout->addLayout(btn_layout);

    connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::init_output_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *title = new QLabel("录像输出路径");
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    layout->addSpacing(8);

    auto *path_layout = new QHBoxLayout();
    m_output_path_edit = new QLineEdit();
    QString default_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    m_output_path_edit->setText(default_path);
    m_output_path_edit->setReadOnly(true);

    auto *browse_btn = new QPushButton("更改");
    browse_btn->setFixedWidth(60);

    path_layout->addWidget(m_output_path_edit);
    path_layout->addWidget(browse_btn);
    layout->addLayout(path_layout);
    layout->addStretch();

    connect(browse_btn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择录像输出路径",
                                                        m_output_path_edit->text());
        if (!dir.isEmpty()) {
            m_output_path_edit->setText(dir);
        }
    });

    m_page_stack->addWidget(page);
}

void SettingsDialog::init_streaming_page() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *title = new QLabel("推流设置");
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    layout->addSpacing(8);

    auto *url_layout = new QHBoxLayout();
    auto *url_label = new QLabel("推流地址:");
    m_stream_url_edit = new QLineEdit();
    m_stream_url_edit->setPlaceholderText("rtmp://localhost/live/stream");
    url_layout->addWidget(url_label);
    url_layout->addWidget(m_stream_url_edit, 1);
    layout->addLayout(url_layout);

    auto *hint = new QLabel("请输入有效的 RTMP 推流地址");
    hint->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(hint);
    layout->addStretch();

    m_page_stack->addWidget(page);
}

QString SettingsDialog::streaming_url() const {
    return m_stream_url_edit->text().trimmed();
}
