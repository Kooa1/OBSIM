//
// Created by 66 on 2025/8/21.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../../utils/PCH.h"

#include <QSplitter>
#include <QHBoxLayout>
#include <QDateTime>
#include <QPointer>
#include <QStandardPaths>
#include <QFutureWatcher>

#include "scenepreviewwidget.h"
#include "settingbar.h"
#include "controlbar.h"
#include "settingsdialog.h"
#include "filterpreviewwidget.h"
#include "settingspreviewwidget.h"

#include "../../core/audiomanager.h"
#include "../../core/filerecoder.h"
#include "../../core/streampush.h"
#include "../../core/base/videocaptor.h"
#include "../../utils/configmanager.h"
#include "../../core/screencapturesource.h"
#include "../../core/cameracapturesource.h"
#include "../../core/textsource.h"
#include "../../core/imagesource.h"

struct CaptorConfig;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow();

    ~MainWindow() override;

    bool init_UI();

    void connect_audio_signals();

private:
    void init_layout();

    void init_conn();

    void connect_signal();

    void connect_recorder_signals();

    void center_on_primary_screen(QWidget *window);

    void adjust_window_screen(QWidget *window = nullptr,
                              double target_aspect_ratio = 16.0 / 9.0,
                              double screen_occupancy_ratio = 0.78);

    void on_display_capture_requested(const CaptorConfig &config, const QString &name);
    void on_camera_capture_requested(const QString &device_desc, const QString &name);
    void on_text_source_requested(const QString &text, const QFont &font, const QColor &color, const QString &name);
    void on_image_source_requested(const QString &file_path, const QString &name);
    void on_source_remove_requested(int index);
    void on_source_move_up_requested(int row);
    void on_source_move_down_requested(int row);
    void on_source_list_selection_changed(int row);
    void on_canvas_selection_changed(int index);

    void on_scene_added(const QString &name);
    void on_scene_removed(int index);
    void on_scene_selection_changed(int index);
    void rebuild_source_list();

    void on_filter_requested(Source *source);
    void on_settings_requested(Source *source);
    void on_settings_source_name_changed(Source *src, const QString &new_name);

    void on_recording_started(const QString &output_path);
    void on_recording_stopped();

    void on_streaming_started(const QString &rtmp_url);
    void on_streaming_stopped();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void load_settings();
    void save_sources();
    void load_sources();
    QString type_display_suffix(Source *src);

    QVBoxLayout *main_layout = nullptr;

    QSplitter *main_splitter = nullptr;

    ScenePreviewWidget *scene_preview_widget = nullptr;

    SettingBar *setting_bar = nullptr;

    ControlBar *control_bar = nullptr;

    std::unique_ptr<AudioManager> m_audio_manager;
    std::unique_ptr<FileRecoder> m_recoder;
    std::unique_ptr<StreamPush> m_stream_push;

    QPointer<FilterPreviewWidget> m_filter_window;
    QPointer<SettingsPreviewWidget> m_settings_window;
    Source *m_filtered_source = nullptr;
};

#endif //MAINWINDOW_H
