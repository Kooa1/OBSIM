//
// Created by 66 on 2025/8/21.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../PCH.h"

#include <QSplitter>
#include <QHBoxLayout>

#include "UI/sdlwindow.h"
#include "UI/settingbar.h"
#include "controlbar.h"
#include "Core/coreengine.h"
#include "Core/displaycaptor.h"

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(CoreEngine &core_engine);

    ~MainWindow() override;

    bool init_UI();

public slots:

private:
    void init_layout();

    void center_on_primary_screen(QWidget *window);

    void adjust_window_screen(QWidget *window = nullptr,
                              double target_aspect_ratio = 16.0 / 9.0,
                              double screen_occupancy_ratio = 0.78);

protected:
    void showEvent(QShowEvent *event) override;

private:
    QVBoxLayout *main_layout = nullptr;

    QSplitter *main_splitter = nullptr;

    SDLWindow *sdl_window = nullptr;

    SettingBar *setting_bar = nullptr;

    ControlBar *control_bar = nullptr;

    CoreEngine &core;
};

#endif //MAINWINDOW_H
