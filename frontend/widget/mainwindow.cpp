//
// Created by 66 on 2025/8/21.
//

#include "mainwindow.h"

#include <QStandardPaths>
#include <QFutureWatcher>
#include <QPointer>

#include "utils/configmanager.h"
#include "core/screencapturesource.h"
#include "core/cameracapturesource.h"
#include "core/textsource.h"
#include "core/imagesource.h"

MainWindow::MainWindow()
    : QWidget(nullptr) {
    setMouseTracking(true);

    init_UI();
    QThread::currentThread()->setPriority(QThread::HighPriority);
}

MainWindow::~MainWindow() = default;

bool MainWindow::init_UI() {
    main_layout = new QVBoxLayout(this);
    main_splitter = new QSplitter(Qt::Vertical, this);

    scene_preview_widget = new ScenePreviewWidget(this);
    setting_bar = new SettingBar(this);
    control_bar = new ControlBar(this);

    // 初始化音频管理器（独立于视频预览）
    m_audio_manager = std::make_unique<AudioManager>(this);
    m_audio_manager->start_all();

    // 初始化录制器
    m_recoder = std::make_unique<FileRecoder>();
    m_stream_push = std::make_unique<StreamPush>();

    init_layout();
    connect_audio_signals();
    connect_signal();
    connect_recorder_signals();
    load_settings();
    load_sources();

    // 初始化场景列表 UI
    for (int i = 0; i < scene_preview_widget->scene_count(); ++i) {
        control_bar->scene_control()->add_item(scene_preview_widget->scene_name_at(i));
    }
    rebuild_source_list();
    return true;
}

void MainWindow::connect_audio_signals() {
    if (!m_audio_manager || !control_bar) return;

    connect(m_audio_manager.get(), &AudioManager::levels_updated,
            control_bar, &ControlBar::update_audio_levels);

    connect(control_bar->audio_mixer(), &AudioMixerBlock::track_volume_changed,
            this, [this](const QString &name, float volume) {
        if (name == "桌面音频") {
            m_recoder->set_system_volume(volume);
            m_stream_push->set_system_volume(volume);
        } else if (name == "麦克风") {
            m_recoder->set_mic_volume(volume);
            m_stream_push->set_mic_volume(volume);
        }
    });

}

void MainWindow::connect_recorder_signals() {
    if (!control_bar || !scene_preview_widget) return;

    connect(control_bar->stream_record(), &StreamRecordBlock::recording_started,
            this, &MainWindow::on_recording_started);
    connect(control_bar->stream_record(), &StreamRecordBlock::recording_stopped,
            this, &MainWindow::on_recording_stopped);

    connect(control_bar->stream_record(), &StreamRecordBlock::streaming_started,
            this, &MainWindow::on_streaming_started);
    connect(control_bar->stream_record(), &StreamRecordBlock::streaming_stopped,
            this, &MainWindow::on_streaming_stopped);
}

void MainWindow::on_recording_started(const QString &output_path) {
    if (!m_recoder || !scene_preview_widget || !m_audio_manager) return;

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString file_path = output_path + "/recording_" + timestamp + ".mp4";

    // 启用音频录制队列
    m_audio_manager->enable_recording();

    // 启动录制器
    m_recoder->start(file_path.toStdString(),
                     static_cast<int>(ScenePreviewWidget::CANVAS_W),
                     static_cast<int>(ScenePreviewWidget::CANVAS_H),
                     30,
                     m_audio_manager->system_record_queue(),
                     m_audio_manager->mic_record_queue());

    // 设置 OpenGL 帧捕获回调
    scene_preview_widget->set_frame_capture_callback(
        [this](const uint8_t *data, int stride, int w, int h) {
            if (m_recoder && m_recoder->is_recording()) {
                m_recoder->feed_frame(data, stride, w, h);
            }
        });

    av_log(nullptr, AV_LOG_INFO, "recording started: %s\n", file_path.toUtf8().constData());
}

void MainWindow::on_recording_stopped() {
    if (!m_recoder || !scene_preview_widget) return;

    scene_preview_widget->set_frame_capture_callback(nullptr);

    m_recoder->stop();

    m_audio_manager->disable_recording();

    av_log(nullptr, AV_LOG_INFO, "recording stopped\n");
}

void MainWindow::on_streaming_started(const QString &rtmp_url) {
    if (!m_stream_push || !scene_preview_widget || !m_audio_manager) return;

    m_audio_manager->enable_recording();

    m_stream_push->start(rtmp_url.toStdString(),
                         static_cast<int>(ScenePreviewWidget::CANVAS_W),
                         static_cast<int>(ScenePreviewWidget::CANVAS_H),
                         30,
                         m_audio_manager->system_record_queue(),
                         m_audio_manager->mic_record_queue());

    scene_preview_widget->set_frame_capture_callback(
        [this](const uint8_t *data, int stride, int w, int h) {
            if (m_stream_push && m_stream_push->is_recording()) {
                m_stream_push->feed_frame(data, stride, w, h);
            }
        });

    av_log(nullptr, AV_LOG_INFO, "streaming started: %s\n", rtmp_url.toUtf8().constData());
}

void MainWindow::on_streaming_stopped() {
    if (!m_stream_push || !scene_preview_widget) return;

    scene_preview_widget->set_frame_capture_callback(nullptr);

    m_stream_push->stop();

    m_audio_manager->disable_recording();

    av_log(nullptr, AV_LOG_INFO, "streaming stopped\n");
}

void MainWindow::load_settings() {
    struct GeneralData {
        QString output_path;
        QString stream_url;
    };

    QFuture<GeneralData> future =
        AsyncSettings::async_load<GeneralData>([](QSettings &settings) {
            GeneralData d;
            d.output_path = settings.value("General/output_path",
                QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString();
            d.stream_url = settings.value("General/stream_url", "").toString();
            return d;
        });

    auto *watcher = new QFutureWatcher<GeneralData>(this);
    QPointer<MainWindow> guard(this);
    connect(watcher, &QFutureWatcher<GeneralData>::finished, this,
            [this, guard, watcher]() {
        if (!guard) {
            watcher->deleteLater();
            return;
        }
        auto d = watcher->result();
        control_bar->stream_record()->set_output_path(d.output_path);
        control_bar->stream_record()->set_stream_url(d.stream_url);
        qDebug() << "已加载设置: output_path =" << d.output_path;
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

struct SourceSaveData {
    QString type;
    QString display_name;
    float pos_x, pos_y;
    float scale_x, scale_y;
    float rotation;
    bool visible, lock_aspect_ratio;
    int offset_x, offset_y, capture_width, capture_height;
    QString text, font_family;
    int font_size;
    QString color_name;
    QString file_path;
};

void MainWindow::save_sources() {
    int scene_count = scene_preview_widget->scene_count();
    if (scene_count == 0) return;

    // 收集所有场景数据
    struct SceneSaveEntry {
        QString name;
        std::vector<SourceSaveData> sources;
    };
    std::vector<SceneSaveEntry> all_data;
    all_data.reserve(scene_count);

    for (int si = 0; si < scene_count; ++si) {
        const SceneData &sd = scene_preview_widget->all_scenes()[si];
        SceneSaveEntry entry;
        entry.name = sd.name;
        entry.sources.reserve(sd.source_storage.size());

        for (const auto &src_ptr : sd.source_storage) {
            Source *src = src_ptr.get();
            SourceSaveData item;
            item.type = QString::fromLatin1(src->source_type_name());
            item.display_name = src->display_name;
            item.pos_x = src->pos_x;
            item.pos_y = src->pos_y;
            item.scale_x = src->scale_x;
            item.scale_y = src->scale_y;
            item.visible = src->visible;
            item.rotation = src->rotation;
            item.lock_aspect_ratio = src->lock_aspect_ratio;

            if (auto *sc = dynamic_cast<ScreenCaptureSource *>(src)) {
                CaptorConfig cfg = sc->captor_config();
                item.offset_x = cfg.offset_x;
                item.offset_y = cfg.offset_y;
                item.capture_width = cfg.width;
                item.capture_height = cfg.height;
            } else if (auto *ts = dynamic_cast<TextSource *>(src)) {
                item.text = ts->text();
                item.font_family = ts->font().family();
                item.font_size = ts->font().pointSize();
                item.color_name = ts->color().name();
            } else if (auto *is = dynamic_cast<ImageSource *>(src)) {
                item.file_path = is->file_path();
            }

            entry.sources.push_back(std::move(item));
        }

        all_data.push_back(std::move(entry));
    }

    int current_idx = scene_preview_widget->current_scene_index();

    AsyncSettings::async_save([all_data = std::move(all_data), current_idx](QSettings &settings) {
        settings.beginGroup("Scenes");
        int scene_count = static_cast<int>(all_data.size());
        settings.setValue("count", scene_count);
        settings.setValue("current_index", current_idx);

        for (int si = 0; si < scene_count; ++si) {
            const auto &entry = all_data[si];
            QString scene_group = QString("Scene_%1").arg(si);
            settings.beginGroup(scene_group);

            settings.setValue("name", entry.name);

            settings.beginGroup("Sources");
            int src_count = static_cast<int>(entry.sources.size());
            settings.setValue("count", src_count);

            for (int i = 0; i < src_count; ++i) {
                const auto &item = entry.sources[i];
                QString group = QString("Source_%1").arg(i);
                settings.beginGroup(group);

                settings.setValue("type", item.type);
                settings.setValue("display_name", item.display_name);
                settings.setValue("pos_x", item.pos_x);
                settings.setValue("pos_y", item.pos_y);
                settings.setValue("scale_x", item.scale_x);
                settings.setValue("scale_y", item.scale_y);
                settings.setValue("visible", item.visible);
                settings.setValue("rotation", item.rotation);
                settings.setValue("lock_aspect_ratio", item.lock_aspect_ratio);

                if (item.type == "Screen Capture") {
                    settings.setValue("offset_x", item.offset_x);
                    settings.setValue("offset_y", item.offset_y);
                    settings.setValue("capture_width", item.capture_width);
                    settings.setValue("capture_height", item.capture_height);
                } else if (item.type == "Text") {
                    settings.setValue("text", item.text);
                    settings.setValue("font_family", item.font_family);
                    settings.setValue("font_size", item.font_size);
                    settings.setValue("color", item.color_name);
                } else if (item.type == "Image") {
                    settings.setValue("file_path", item.file_path);
                }

                settings.endGroup(); // Source_N
            }
            settings.endGroup(); // Sources

            settings.endGroup(); // Scene_N
        }
        settings.endGroup(); // Scenes
    });
}

void MainWindow::load_sources() {
    // 加载数据结构
    struct SceneLoadEntry {
        QString name;
        std::vector<SourceSaveData> sources;
    };
    struct LoadResult {
        std::vector<SceneLoadEntry> scenes;
        int current_index = 0;
    };

    QFuture<LoadResult> future =
        AsyncSettings::async_load<LoadResult>(
            [](QSettings &settings) -> LoadResult {
                LoadResult result;

                // 优先读取新格式 Scenes/
                int scene_count = settings.value("Scenes/count", -1).toInt();
                if (scene_count > 0) {
                    result.current_index = settings.value("Scenes/current_index", 0).toInt();
                    result.scenes.reserve(scene_count);

                    for (int si = 0; si < scene_count; ++si) {
                        QString scene_group = QString("Scenes/Scene_%1").arg(si);
                        settings.beginGroup(scene_group);
                        SceneLoadEntry entry;
                        entry.name = settings.value("name", QString("场景 %1").arg(si + 1)).toString();

                        settings.beginGroup("Sources");
                        int src_count = settings.value("count", 0).toInt();
                        entry.sources.reserve(src_count);

                        for (int i = 0; i < src_count; ++i) {
                            QString group = QString("Source_%1").arg(i);
                            settings.beginGroup(group);
                            SourceSaveData item;
                            item.type = settings.value("type").toString();
                            item.display_name = settings.value("display_name").toString();
                            item.pos_x = settings.value("pos_x", 0.0f).toFloat();
                            item.pos_y = settings.value("pos_y", 0.0f).toFloat();
                            item.scale_x = settings.value("scale_x", 1.0f).toFloat();
                            item.scale_y = settings.value("scale_y", 1.0f).toFloat();
                            item.visible = settings.value("visible", true).toBool();
                            item.rotation = settings.value("rotation", 0.0f).toFloat();
                            item.lock_aspect_ratio = settings.value("lock_aspect_ratio", false).toBool();

                            if (item.type == "Screen Capture") {
                                item.offset_x = settings.value("offset_x").toInt();
                                item.offset_y = settings.value("offset_y").toInt();
                                item.capture_width = settings.value("capture_width").toInt();
                                item.capture_height = settings.value("capture_height").toInt();
                            } else if (item.type == "Text") {
                                item.text = settings.value("text").toString();
                                item.font_family = settings.value("font_family", "Arial").toString();
                                item.font_size = settings.value("font_size", 48).toInt();
                                item.color_name = settings.value("color", "#FFFFFF").toString();
                            } else if (item.type == "Image") {
                                item.file_path = settings.value("file_path").toString();
                            } else if (item.type == "Camera") {
                            }

                            entry.sources.push_back(std::move(item));
                            settings.endGroup(); // Source_N
                        }
                        settings.endGroup(); // Sources
                        settings.endGroup(); // Scene_N

                        result.scenes.push_back(std::move(entry));
                    }
                    return result;
                }

                // 兼容旧格式：Sources/ 组 → 迁移为场景 0
                settings.beginGroup("Sources");
                int count = settings.value("count", 0).toInt();
                if (count > 0) {
                    SceneLoadEntry entry;
                    entry.name = "场景 1";
                    entry.sources.reserve(count);

                    for (int i = 0; i < count; ++i) {
                        QString group = QString("Source_%1").arg(i);
                        settings.beginGroup(group);
                        SourceSaveData item;
                        item.type = settings.value("type").toString();
                        item.display_name = settings.value("display_name").toString();
                        item.pos_x = settings.value("pos_x", 0.0f).toFloat();
                        item.pos_y = settings.value("pos_y", 0.0f).toFloat();
                        item.scale_x = settings.value("scale_x", 1.0f).toFloat();
                        item.scale_y = settings.value("scale_y", 1.0f).toFloat();
                        item.visible = settings.value("visible", true).toBool();
                        item.rotation = settings.value("rotation", 0.0f).toFloat();
                        item.lock_aspect_ratio = settings.value("lock_aspect_ratio", false).toBool();

                        if (item.type == "Screen Capture") {
                            item.offset_x = settings.value("offset_x").toInt();
                            item.offset_y = settings.value("offset_y").toInt();
                            item.capture_width = settings.value("capture_width").toInt();
                            item.capture_height = settings.value("capture_height").toInt();
                        } else if (item.type == "Text") {
                            item.text = settings.value("text").toString();
                            item.font_family = settings.value("font_family", "Arial").toString();
                            item.font_size = settings.value("font_size", 48).toInt();
                            item.color_name = settings.value("color", "#FFFFFF").toString();
                        } else if (item.type == "Image") {
                            item.file_path = settings.value("file_path").toString();
                        } else if (item.type == "Camera") {
                        }

                        entry.sources.push_back(std::move(item));
                        settings.endGroup();
                    }
                    result.scenes.push_back(std::move(entry));
                }
                settings.endGroup(); // Sources
                return result;
            }
        );

    auto *watcher = new QFutureWatcher<LoadResult>(this);
    QPointer<MainWindow> guard(this);
    connect(watcher, &QFutureWatcher<LoadResult>::finished, this,
            [this, guard, watcher]() {
        if (!guard) {
            watcher->deleteLater();
            return;
        }
        auto result = watcher->result();

        if (result.scenes.empty()) {
            watcher->deleteLater();
            return; // 保留默认场景
        }

        // 清空现有场景，用加载数据重建
        scene_preview_widget->clear_all_scenes();

        for (auto &entry : result.scenes) {
            int scene_idx = scene_preview_widget->add_scene(entry.name);
            scene_preview_widget->switch_to_scene(scene_idx);

            for (const auto &item : entry.sources) {
                if (item.type == "Screen Capture") {
                    CaptorConfig config;
                    config.offset_x = item.offset_x;
                    config.offset_y = item.offset_y;
                    config.width = item.capture_width;
                    config.height = item.capture_height;
                    scene_preview_widget->add_screen_capture_source(config, item.display_name);
                } else if (item.type == "Camera") {
                    scene_preview_widget->add_camera_capture_source(item.display_name);
                } else if (item.type == "Text") {
                    QFont font;
                    font.setFamily(item.font_family);
                    font.setPointSize(item.font_size);
                    QColor color(item.color_name);
                    scene_preview_widget->add_text_source(item.text, font, color, item.display_name);
                } else if (item.type == "Image") {
                    scene_preview_widget->add_image_source(item.file_path, item.display_name);
                }

                Source *src = scene_preview_widget->source_at(
                    scene_preview_widget->source_count() - 1);
                src->pos_x = item.pos_x;
                src->pos_y = item.pos_y;
                src->scale_x = item.scale_x;
                src->scale_y = item.scale_y;
                src->visible = item.visible;
                src->rotation = item.rotation;
                src->lock_aspect_ratio = item.lock_aspect_ratio;
            }
        }

        // 恢复当前场景
        int restore_idx = std::clamp(result.current_index, 0,
                                     scene_preview_widget->scene_count() - 1);
        scene_preview_widget->switch_to_scene(restore_idx);

        // 重建场景列表 UI
        QListWidget *scene_list = control_bar->scene_control()->scene_list();
        scene_list->clear();
        for (int i = 0; i < scene_preview_widget->scene_count(); ++i) {
            scene_list->addItem(scene_preview_widget->scene_name_at(i));
        }
        scene_list->setCurrentRow(restore_idx);

        // 重建源列表 UI
        QListWidget *list = control_bar->source_control()->source_list();
        list->clear();
        for (int i = static_cast<int>(scene_preview_widget->source_count()) - 1; i >= 0; --i) {
            Source *src = scene_preview_widget->source_at(i);
            list->addItem(src->display_name + " (" + type_display_suffix(src) + ")");
        }

        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

QString MainWindow::type_display_suffix(Source *src) {
    if (dynamic_cast<ScreenCaptureSource *>(src)) return QStringLiteral("显示屏采集");
    if (dynamic_cast<CameraCaptureSource *>(src)) return QStringLiteral("摄像头采集");
    if (dynamic_cast<TextSource *>(src)) return QStringLiteral("文字源");
    if (dynamic_cast<ImageSource *>(src)) return QStringLiteral("图片源");
    return QStringLiteral("未知");
}

void MainWindow::connect_signal() {
    if (!control_bar || !scene_preview_widget) return;

    connect(control_bar->source_control(), &SourceControlBlock::display_capture_requested,
            this, &MainWindow::on_display_capture_requested);
    connect(control_bar->source_control(), &SourceControlBlock::camera_capture_requested,
            this, &MainWindow::on_camera_capture_requested);
    connect(control_bar->source_control(), &SourceControlBlock::text_source_requested,
            this, &MainWindow::on_text_source_requested);
    connect(control_bar->source_control(), &SourceControlBlock::image_source_requested,
            this, &MainWindow::on_image_source_requested);
    connect(control_bar->source_control(), &SourceControlBlock::source_remove_requested,
            this, &MainWindow::on_source_remove_requested);
    connect(control_bar->source_control(), &SourceControlBlock::source_move_up_requested,
            this, &MainWindow::on_source_move_up_requested);
    connect(control_bar->source_control(), &SourceControlBlock::source_move_down_requested,
            this, &MainWindow::on_source_move_down_requested);
    connect(control_bar->source_control(), &SourceControlBlock::source_list_selection_changed,
            this, &MainWindow::on_source_list_selection_changed);
    connect(scene_preview_widget, &ScenePreviewWidget::canvas_selection_changed,
            this, &MainWindow::on_canvas_selection_changed);
    connect(scene_preview_widget, &ScenePreviewWidget::source_position_changed,
            this, &MainWindow::save_sources);

    // 场景控制信号
    connect(control_bar->scene_control(), &SceneControlBlock::scene_added,
            this, &MainWindow::on_scene_added);
    connect(control_bar->scene_control(), &SceneControlBlock::scene_removed,
            this, &MainWindow::on_scene_removed);
    connect(control_bar->scene_control(), &SceneControlBlock::scene_selection_changed,
            this, &MainWindow::on_scene_selection_changed);
}

void MainWindow::on_display_capture_requested(const CaptorConfig &config, const QString &name) {
    if (scene_preview_widget) {
        scene_preview_widget->add_screen_capture_source(config, name);
        save_sources();
    }
}

void MainWindow::on_camera_capture_requested(const QString &device_desc, const QString &name) {
    if (scene_preview_widget) {
        scene_preview_widget->add_camera_capture_source(name, device_desc.toStdString());
        save_sources();
    }
}

void MainWindow::on_text_source_requested(const QString &text, const QFont &font, const QColor &color, const QString &name) {
    if (scene_preview_widget) {
        scene_preview_widget->add_text_source(text, font, color, name);
        save_sources();
    }
}

void MainWindow::on_image_source_requested(const QString &file_path, const QString &name) {
    if (scene_preview_widget) {
        scene_preview_widget->add_image_source(file_path, name);
        save_sources();
    }
}

void MainWindow::on_source_remove_requested(int index) {
    if (scene_preview_widget) {
        scene_preview_widget->remove_source(index);
        save_sources();
    }
}

void MainWindow::on_source_move_up_requested(int scene_idx) {
    if (scene_preview_widget) {
        scene_preview_widget->move_source_up(scene_idx);
        QListWidget *list = control_bar->source_control()->source_list();
        int N = list->count();
        int list_row = N - 1 - scene_idx;
        if (list_row > 0) {
            QListWidgetItem *item = list->takeItem(list_row);
            list->insertItem(list_row - 1, item);
            list->setCurrentRow(list_row - 1);
        }
        save_sources();
    }
}

void MainWindow::on_source_move_down_requested(int scene_idx) {
    if (scene_preview_widget) {
        scene_preview_widget->move_source_down(scene_idx);
        QListWidget *list = control_bar->source_control()->source_list();
        int N = list->count();
        int list_row = N - 1 - scene_idx;
        if (list_row < N - 1) {
            QListWidgetItem *item = list->takeItem(list_row);
            list->insertItem(list_row + 1, item);
            list->setCurrentRow(list_row + 1);
        }
        save_sources();
    }
}

void MainWindow::on_source_list_selection_changed(int row) {
    if (scene_preview_widget) {
        scene_preview_widget->select_source_at(row);
    }
}

void MainWindow::on_canvas_selection_changed(int scene_idx) {
    if (control_bar && control_bar->source_control()) {
        QListWidget *list = control_bar->source_control()->source_list();
        int list_row = list->count() - 1 - scene_idx;
        list->setCurrentRow(list_row);
    }
}

void MainWindow::on_scene_added(const QString &name) {
    if (!scene_preview_widget) return;
    int new_index = scene_preview_widget->add_scene(name);
    scene_preview_widget->switch_to_scene(new_index);
    rebuild_source_list();
    save_sources();
}

void MainWindow::on_scene_removed(int index) {
    if (!scene_preview_widget) return;
    scene_preview_widget->remove_scene(index);

    // 删除场景后同步 UI
    rebuild_source_list();
    save_sources();
}

void MainWindow::on_scene_selection_changed(int index) {
    if (!scene_preview_widget) return;
    scene_preview_widget->switch_to_scene(index);

    // 切换场景时重建源列表
    rebuild_source_list();
}

void MainWindow::rebuild_source_list() {
    if (!control_bar || !scene_preview_widget) return;

    QListWidget *list = control_bar->source_control()->source_list();
    list->clear();

    for (int i = static_cast<int>(scene_preview_widget->source_count()) - 1; i >= 0; --i) {
        Source *src = scene_preview_widget->source_at(i);
        list->addItem(src->display_name + " (" + type_display_suffix(src) + ")");
    }
}

void MainWindow::init_layout() {
    main_splitter->addWidget(scene_preview_widget);
    main_splitter->addWidget(setting_bar);
    main_splitter->addWidget(control_bar);
    main_layout->addWidget(main_splitter);

    if (const auto handle = main_splitter->handle(1); handle) {
        handle->setEnabled(false);
        handle->setVisible(false);
    }

    main_splitter->setCollapsible(0, false);
    main_splitter->setCollapsible(1, false);
    main_splitter->setCollapsible(2, false);

    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 0);
    main_splitter->setStretchFactor(2, 0);

    adjust_window_screen(this);
    this->show();
}

void MainWindow::center_on_primary_screen(QWidget *window) {
    if (!window) return;

    // 获取主屏幕
    const QScreen *primary_screen = QGuiApplication::primaryScreen();
    if (!primary_screen) return;

    // 获取主屏幕的可用区域（排除任务栏等）
    const QRect screenGeometry = primary_screen->availableGeometry();

    // 获取窗口大小
    const QSize windowSize = window->size();

    // 计算居中位置
    const int x = screenGeometry.x() + (screenGeometry.width() - windowSize.width()) / 2;
    const int y = screenGeometry.y() + (screenGeometry.height() - windowSize.height()) / 2;

    // 移动窗口到中心位置
    window->move(x, y);
}

void MainWindow::adjust_window_screen(QWidget *window, double target_aspect_ratio, double screen_occupancy_ratio) {
    if (!window) return;

    // 1. 获取主屏幕可用区域（排除任务栏等）
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();

    // 屏幕原始大小（调试用）
    QRect screen_rect = screen->geometry();

    // 2. 计算目标窗口高度（占可用高度的百分比）
    int target_height = static_cast<int>(available.height() * screen_occupancy_ratio);

    // 3. 根据宽高比计算目标宽度
    int target_width = static_cast<int>(target_height * target_aspect_ratio);

    // 4. 边界检查：如果计算出的宽度超过屏幕可用宽度，则以宽度为准重新计算
    if (target_width > available.width() * 0.95) {
        target_width = static_cast<int>(available.width() * 0.95);
        target_height = static_cast<int>(target_width / target_aspect_ratio);
    }

    // 5. 设置窗口最小尺寸（防止过小）
    constexpr int min_width = 1024;
    constexpr int min_height = 768;
    window->setMinimumSize(min_width, min_height);

    // 6. 应用尺寸
    window->resize(target_width, target_height);

    // 7. 居中显示
    const int x = available.x() + (available.width() - target_width) / 2;
    const int y = available.y() + (available.height() - target_height) / 2;
    window->move(x, y);
}

void MainWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    center_on_primary_screen(this);
}
