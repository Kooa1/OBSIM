#include "controlbar.h"
#include "../core/textsource.h"
#include "../core/videocaptor.h"

#include <QColorDialog>
#include <QFontComboBox>
#include <QFileDialog>

// ==================== 场景控制块 ====================

SceneControlBlock::SceneControlBlock(QWidget *parent)
        : ControlBlock("场景", parent) {
    m_scene_list = new QListWidget();
    m_scene_list->setAlternatingRowColors(true);

    auto *btn_layout = new QHBoxLayout();
    auto *btn_add = new QPushButton("＋");
    auto *btn_del = new QPushButton("－");
    btn_add->setFixedSize(28, 28);
    btn_del->setFixedSize(28, 28);
    btn_layout->addStretch();
    btn_layout->addWidget(btn_add);
    btn_layout->addWidget(btn_del);

    m_content_layout->addWidget(m_scene_list);
    m_content_layout->addLayout(btn_layout);

    m_scene_list->addItem("场景 1");
    m_scene_list->addItem("场景 2");
    m_scene_list->setCurrentRow(0);
}


// ==================== 输入源控制块 ====================

SourceControlBlock::SourceControlBlock(QWidget *parent)
        : ControlBlock("输入源", parent) {
    m_source_list = new QListWidget();
    m_source_list->setAlternatingRowColors(true);

    auto *btn_layout = new QHBoxLayout();
    auto *btn_add = new QPushButton("＋");
    auto *btn_del = new QPushButton("－");
    btn_add->setFixedSize(28, 28);
    btn_del->setFixedSize(28, 28);
    btn_layout->addStretch();
    btn_layout->addWidget(btn_add);
    btn_layout->addWidget(btn_del);

    m_content_layout->addWidget(m_source_list);
    m_content_layout->addLayout(btn_layout);

    connect(m_source_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int row = m_source_list->row(item);
        emit source_list_selection_changed(row);
    });

    connect(btn_del, &QPushButton::clicked, this, [this]() {
        int row = m_source_list->currentRow();
        if (row < 0) return;
        delete m_source_list->takeItem(row);
        emit source_remove_requested(row);
    });

    connect(btn_add, &QPushButton::clicked, this, [this]() {
        SourceTypeDialog type_dialog;
        if (type_dialog.exec() != QDialog::Accepted) return;

        QString type = type_dialog.selected_type();

        if (type == QStringLiteral("文字源")) {
            TextSourceDialog text_dialog;
            if (text_dialog.exec() == QDialog::Accepted) {
                QString name = text_dialog.source_name();
                QString content = text_dialog.text_content();
                if (name.isEmpty() || content.isEmpty()) return;
                emit text_source_requested(content, text_dialog.selected_font(),
                                           text_dialog.selected_color(), name);
                m_source_list->addItem(name + " (" + type + ")");
            }
            return;
        }

        if (type == QStringLiteral("图片源")) {
            ImageSourceDialog img_dialog;
            if (img_dialog.exec() == QDialog::Accepted) {
                QString name = img_dialog.source_name();
                QString path = img_dialog.file_path();
                if (name.isEmpty() || path.isEmpty()) return;
                emit image_source_requested(path, name);
                m_source_list->addItem(name + " (" + type + ")");
            }
            return;
        }

        QVector<DisplayInfo> displays;
        if (type == QStringLiteral("显示屏采集")) {
            DeviceManager dm;
            displays = dm.get_all_displays();
        }

        QVector<CameraInfo> cameras;
        if (type == QStringLiteral("摄像头采集")) {
            DeviceManager dm;
            cameras = dm.get_all_cameras();
        }

        SourceNameDialog name_dialog(displays, cameras, type == QStringLiteral("摄像头采集"));
        if (name_dialog.exec() == QDialog::Accepted) {
            QString name = name_dialog.source_name();
            if (name.isEmpty()) return;

            if (type == QStringLiteral("显示屏采集")) {
                int idx = name_dialog.selected_display_index();
                if (idx >= 0 && idx < displays.size()) {
                    CaptorConfig config;
                    config.offset_x = displays[idx].geometry.x();
                    config.offset_y = displays[idx].geometry.y();
                    config.width = displays[idx].geometry.width();
                    config.height = displays[idx].geometry.height();
                    emit display_capture_requested(config, name);
                    m_source_list->addItem(name + " (" + type + ")");
                }
            } else {
                int idx = name_dialog.selected_camera_index();
                if (idx >= 0 && idx < cameras.size()) {
                    emit camera_capture_requested(
                        QString::fromStdString(cameras[idx].name), name);
                    m_source_list->addItem(name + " (" + type + ")");
                }
                // idx < 0 表示用户未选择有效摄像头，不创建采集源
            }
        }
    });
}


// ==================== 混音控制块 ====================

AudioMixerBlock::AudioMixerBlock(QWidget *parent)
        : ControlBlock("混音器", parent) {
    m_tracks_layout = new QVBoxLayout();
    m_tracks_layout->setSpacing(6);
    m_content_layout->addLayout(m_tracks_layout);
    m_content_layout->addStretch();

    // 默认添加几条音轨
    add_track("桌面音频", 0.8f);
    add_track("麦克风", 0.6f);
}

AudioMixerBlock::TrackWidget AudioMixerBlock::create_track_widget(const QString &name, float volume) {
    TrackWidget tw;

    tw.container = new QWidget();
    auto *layout = new QVBoxLayout(tw.container);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(2);

    // 第一行：名称 + 静音按钮
    auto *top_row = new QHBoxLayout();
    tw.name_label = new QLabel(name);

    tw.mute_btn = new QPushButton("🔊");
    tw.mute_btn->setFixedSize(24, 24);
    tw.mute_btn->setCheckable(true);

    top_row->addWidget(tw.name_label);
    top_row->addStretch();
    top_row->addWidget(tw.mute_btn);

    // 第二行：音量滑块
    tw.volume_slider = new QSlider(Qt::Horizontal);
    tw.volume_slider->setRange(0, 100);
    tw.volume_slider->setValue(static_cast<int>(volume * 100));

    // 第三行：电平表（带颜色渐变效果）
    tw.level_meter = new QProgressBar();
    tw.level_meter->setRange(0, 100);
    tw.level_meter->setValue(0);
    tw.level_meter->setTextVisible(false);
    tw.level_meter->setFixedHeight(8);

    // 动态颜色样式：低电平绿色，中电平黄色，高电平红色
    tw.level_meter->setStyleSheet(
            "QProgressBar {"
            "  background-color: #2a2a2a;"
            "  border: 1px solid #555;"
            "  border-radius: 2px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "    stop:0 #4CAF50, stop:0.6 #FFEB3B, stop:0.85 #FF9800, stop:1 #F44336);"
            "  border-radius: 1px;"
            "}"
    );

    layout->addLayout(top_row);
    layout->addWidget(tw.volume_slider);
    layout->addWidget(tw.level_meter);

    // 信号连接
    QObject::connect(tw.volume_slider, &QSlider::valueChanged, this,
                     [this, name](int value) {
                         emit track_volume_changed(name, value / 100.0f);
                     });

    QObject::connect(tw.mute_btn, &QPushButton::toggled, this,
                     [this, name](bool checked) {
                         emit track_muted_changed(name, checked);
                     });

    return tw;
}

void AudioMixerBlock::add_track(const QString &name, float volume) {
    if (m_tracks.contains(name)) return;

    TrackWidget tw = create_track_widget(name, volume);
    m_tracks_layout->addWidget(tw.container);
    m_tracks[name] = tw;
}

void AudioMixerBlock::remove_track(const QString &name) {
    auto it = m_tracks.find(name);
    if (it != m_tracks.end()) {
        m_tracks_layout->removeWidget(it->second.container);
        delete it->second.container;
        m_tracks.erase(it);
    }
}

void AudioMixerBlock::clear_tracks() {
    for (auto &[name, tw]: m_tracks) {
        m_tracks_layout->removeWidget(tw.container);
        delete tw.container;
    }
    m_tracks.clear();
}

void AudioMixerBlock::update_track_level(const QString &name, float level) {
    auto it = m_tracks.find(name);
    if (it != m_tracks.end()) {
        int value = static_cast<int>(level * 100);
        // 用 QMetaObject::invokeMethod 确保 UI 更新在主线程
        QMetaObject::invokeMethod(it->second.level_meter, "setValue",
                                  Qt::QueuedConnection, Q_ARG(int, value));
    }
}


// ==================== 直播录制块 ====================

StreamRecordBlock::StreamRecordBlock(QWidget *parent)
        : ControlBlock("直播 / 录制", parent) {
    m_btn_start_stream = new QPushButton("🔴 开始直播");
    m_btn_start_record = new QPushButton("⏺ 开始录制");
    m_btn_settings = new QPushButton("⚙ 设置");
    m_btn_exit = new QPushButton("✕ 退出");

    m_content_layout->addWidget(m_btn_start_stream);
    m_content_layout->addWidget(m_btn_start_record);
    m_content_layout->addStretch();
    m_content_layout->addWidget(m_btn_settings);
    m_content_layout->addWidget(m_btn_exit);

    connect(m_btn_start_stream, &QPushButton::clicked, this, &StreamRecordBlock::start_stream_clicked);
    connect(m_btn_start_record, &QPushButton::clicked, this, &StreamRecordBlock::start_record_clicked);
    connect(m_btn_exit, &QPushButton::clicked, qApp, &QApplication::quit);
}


// ==================== 文字源配置对话框 ====================

TextSourceDialog::TextSourceDialog(QWidget *parent)
        : QDialog(parent), m_color(Qt::white) {
    setWindowTitle("添加文字源");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *name_label = new QLabel("输入源名称:");
    m_name_edit = new QLineEdit();
    m_name_edit->setPlaceholderText("请输入文字源名称");
    layout->addWidget(name_label);
    layout->addWidget(m_name_edit);

    auto *content_label = new QLabel("文字内容:");
    m_text_content = new QPlainTextEdit();
    m_text_content->setPlaceholderText("请输入要显示的文字内容");
    m_text_content->setFixedHeight(80);
    layout->addWidget(content_label);
    layout->addWidget(m_text_content);

    auto *font_layout = new QHBoxLayout();

    auto *font_group = new QGroupBox("字体");
    auto *font_vbox = new QVBoxLayout(font_group);
    m_font_combo = new QFontComboBox();
    m_font_combo->setCurrentFont(QFont("Arial"));
    font_vbox->addWidget(m_font_combo);
    font_layout->addWidget(font_group);

    auto *right_group = new QGroupBox("大小 / 颜色");
    auto *right_grid = new QVBoxLayout(right_group);

    auto *size_layout = new QHBoxLayout();
    size_layout->addWidget(new QLabel("大小:"));
    m_size_spin = new QSpinBox();
    m_size_spin->setRange(8, 300);
    m_size_spin->setValue(48);
    size_layout->addWidget(m_size_spin);
    right_grid->addLayout(size_layout);

    auto *color_layout = new QHBoxLayout();
    color_layout->addWidget(new QLabel("颜色:"));
    m_color_btn = new QPushButton();
    m_color_btn->setFixedHeight(28);
    m_color_btn->setMinimumWidth(60);
    m_color_btn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #888; border-radius: 3px;")
            .arg(m_color.name()));
    color_layout->addWidget(m_color_btn);
    right_grid->addLayout(color_layout);

    font_layout->addWidget(right_group);
    layout->addLayout(font_layout);

    auto *button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok_btn = button_box->button(QDialogButtonBox::Ok);
    ok_btn->setEnabled(false);

    auto update_ok_btn = [this, ok_btn]() {
        bool name_valid = !m_name_edit->text().trimmed().isEmpty();
        bool content_valid = !m_text_content->toPlainText().trimmed().isEmpty();
        ok_btn->setEnabled(name_valid && content_valid);
    };
    connect(m_name_edit, &QLineEdit::textChanged, this, update_ok_btn);
    connect(m_text_content, &QPlainTextEdit::textChanged, this, update_ok_btn);

    connect(m_color_btn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_color, this, "选择字体颜色");
        if (c.isValid()) {
            m_color = c;
            m_color_btn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #888;")
                    .arg(m_color.name()));
        }
    });

    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addStretch();
    layout->addWidget(button_box);
}

QString TextSourceDialog::source_name() const {
    return m_name_edit->text().trimmed();
}

QString TextSourceDialog::text_content() const {
    return m_text_content->toPlainText().trimmed();
}

QFont TextSourceDialog::selected_font() const {
    QFont font = m_font_combo->currentFont();
    font.setPointSize(m_size_spin->value());
    return font;
}

QColor TextSourceDialog::selected_color() const {
    return m_color;
}


// ==================== 图片源配置对话框 ====================

ImageSourceDialog::ImageSourceDialog(QWidget *parent)
        : QDialog(parent) {
    setWindowTitle("添加图片源");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *name_label = new QLabel("输入源名称:");
    m_name_edit = new QLineEdit();
    m_name_edit->setPlaceholderText("请输入图片源名称");
    layout->addWidget(name_label);
    layout->addWidget(m_name_edit);

    auto *path_label = new QLabel("图片路径:");
    auto *path_layout = new QHBoxLayout();
    m_path_edit = new QLineEdit();
    m_path_edit->setPlaceholderText("请选择图片文件");
    m_path_edit->setReadOnly(true);
    auto *browse_btn = new QPushButton("浏览...");
    browse_btn->setFixedWidth(80);
    path_layout->addWidget(m_path_edit);
    path_layout->addWidget(browse_btn);
    layout->addWidget(path_label);
    layout->addLayout(path_layout);

    connect(browse_btn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "选择图片",
                                                     QString(),
                                                     "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
        if (!path.isEmpty()) {
            m_path_edit->setText(path);
        }
    });

    auto *button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok_btn = button_box->button(QDialogButtonBox::Ok);
    ok_btn->setEnabled(false);

    auto update_ok_btn = [this, ok_btn]() {
        bool name_valid = !m_name_edit->text().trimmed().isEmpty();
        bool path_valid = !m_path_edit->text().trimmed().isEmpty();
        ok_btn->setEnabled(name_valid && path_valid);
    };
    connect(m_name_edit, &QLineEdit::textChanged, this, update_ok_btn);
    connect(m_path_edit, &QLineEdit::textChanged, this, update_ok_btn);

    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addStretch();
    layout->addWidget(button_box);
}

QString ImageSourceDialog::source_name() const {
    return m_name_edit->text().trimmed();
}

QString ImageSourceDialog::file_path() const {
    return m_path_edit->text().trimmed();
}


// ==================== 输入源类型选择对话框 ====================

SourceTypeDialog::SourceTypeDialog(QWidget *parent)
        : QDialog(parent) {
    setWindowTitle("选择输入源类型");
    setMinimumWidth(280);

    auto *layout = new QVBoxLayout(this);

    auto *btn_text = new QPushButton("文字源");
    auto *btn_image = new QPushButton("图片源");
    auto *btn_camera = new QPushButton("摄像头采集");
    auto *btn_display = new QPushButton("显示屏采集");
    btn_text->setFixedHeight(48);
    btn_image->setFixedHeight(48);
    btn_camera->setFixedHeight(48);
    btn_display->setFixedHeight(48);

    connect(btn_text, &QPushButton::clicked, this, [this]() {
        m_selected_type = QStringLiteral("文字源");
        accept();
    });
    connect(btn_image, &QPushButton::clicked, this, [this]() {
        m_selected_type = QStringLiteral("图片源");
        accept();
    });
    connect(btn_camera, &QPushButton::clicked, this, [this]() {
        m_selected_type = QStringLiteral("摄像头采集");
        accept();
    });
    connect(btn_display, &QPushButton::clicked, this, [this]() {
        m_selected_type = QStringLiteral("显示屏采集");
        accept();
    });

    layout->addWidget(btn_text);
    layout->addWidget(btn_image);
    layout->addWidget(btn_camera);
    layout->addWidget(btn_display);
}


// ==================== 输入源命名对话框 ====================

SourceNameDialog::SourceNameDialog(const QVector<DisplayInfo> &displays,
                                   const QVector<CameraInfo> &cameras,
                                   bool is_camera_type, QWidget *parent)
        : QDialog(parent) {
    setWindowTitle("输入源名称");
    setMinimumWidth(350);

    auto *layout = new QVBoxLayout(this);

    auto *name_label = new QLabel("输入源名称:");
    m_name_edit = new QLineEdit();
    m_name_edit->setPlaceholderText("请输入输入源名称");
    layout->addWidget(name_label);
    layout->addWidget(m_name_edit);

    if (!displays.isEmpty()) {
        auto *display_label = new QLabel("选择显示器:");
        m_display_combo = new QComboBox();
        for (const auto &d: displays) {
            QString text = QString::fromStdString(d.name);
            if (d.is_primary) {
                text = "主屏幕: " + text;
            }
            m_display_combo->addItem(text, d.index);
        }
        layout->addWidget(display_label);
        layout->addWidget(m_display_combo);
    }

    if (is_camera_type) {
        auto *camera_label = new QLabel("选择摄像头:");
        m_camera_combo = new QComboBox();
        m_camera_combo->addItem("无", -1);
        for (const auto &c: cameras) {
            QString text = QString::fromStdString(c.name);
            if (c.is_default) {
                text = "默认: " + text;
            }
            m_camera_combo->addItem(text, c.index);
        }
        layout->addWidget(camera_label);
        layout->addWidget(m_camera_combo);
    }

    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok_btn = button_box->button(QDialogButtonBox::Ok);
    ok_btn->setEnabled(false);

    // 名称文本变化时更新确定按钮状态
    auto update_ok_btn = [this, ok_btn, is_camera_type]() {
        bool name_valid = !m_name_edit->text().trimmed().isEmpty();
        bool camera_valid = !is_camera_type ||
                            (m_camera_combo && m_camera_combo->currentData().toInt() >= 0);
        ok_btn->setEnabled(name_valid && camera_valid);
    };

    connect(m_name_edit, &QLineEdit::textChanged, this, update_ok_btn);

    if (is_camera_type) {
        connect(m_camera_combo, &QComboBox::currentIndexChanged, this, update_ok_btn);
    }

    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addStretch();
    layout->addWidget(button_box);
}

int SourceNameDialog::selected_display_index() const {
    if (m_display_combo) {
        return m_display_combo->currentData().toInt();
    }
    return -1;
}

int SourceNameDialog::selected_camera_index() const {
    if (m_camera_combo) {
        return m_camera_combo->currentData().toInt();
    }
    return -1;
}


// ==================== 总控制栏 ====================

ControlBar::ControlBar(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(220);
    init_UI();
}

void ControlBar::update_audio_levels(float system_level, float mic_level) {
    if (m_audio_mixer_block) {
        m_audio_mixer_block->update_track_level("桌面音频", system_level);
        m_audio_mixer_block->update_track_level("麦克风", mic_level);
    }
}

void ControlBar::init_UI() {
    main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(6, 6, 6, 6);

    main_splitter = new QSplitter(Qt::Horizontal, this);

    init_control_blocks();
    init_layout();
}

void ControlBar::init_control_blocks() {
    m_scene_block = new SceneControlBlock(this);
    m_source_block = new SourceControlBlock(this);
    m_audio_mixer_block = new AudioMixerBlock(this);
    m_stream_record_block = new StreamRecordBlock(this);
}

void ControlBar::init_layout() {
    // 场景 | 输入源 | 混音器 | 直播录制
    main_splitter->addWidget(m_scene_block);
    main_splitter->addWidget(m_source_block);
    main_splitter->addWidget(m_audio_mixer_block);
    main_splitter->addWidget(m_stream_record_block);

    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 4);
    main_splitter->setStretchFactor(3, 2);

    main_layout->addWidget(main_splitter);
}
