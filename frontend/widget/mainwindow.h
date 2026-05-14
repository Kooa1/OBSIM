//
// Created by 66 on 2025/8/21.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../utils/PCH.h"

#include <QSplitter>
#include <QHBoxLayout>

#include "scenepreviewwidget.h"
#include "settingbar.h"
#include "controlbar.h"

#include "../core/audiomanager.h"

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

    void center_on_primary_screen(QWidget *window);

    void adjust_window_screen(QWidget *window = nullptr,
                              double target_aspect_ratio = 16.0 / 9.0,
                              double screen_occupancy_ratio = 0.78);

    void on_display_capture_requested(const CaptorConfig &config, const QString &name);
    void on_camera_capture_requested(const QString &device_desc, const QString &name);
    void on_source_remove_requested(int index);
    void on_source_list_selection_changed(int row);
    void on_canvas_selection_changed(int index);

protected:
    void showEvent(QShowEvent *event) override;

private:
    QVBoxLayout *main_layout = nullptr;

    QSplitter *main_splitter = nullptr;

    ScenePreviewWidget *scene_preview_widget = nullptr;

    SettingBar *setting_bar = nullptr;

    ControlBar *control_bar = nullptr;

    std::unique_ptr<AudioManager> m_audio_manager;
};

#endif //MAINWINDOW_H
