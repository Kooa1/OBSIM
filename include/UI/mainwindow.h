//
// Created by 66 on 2025/8/21.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PCH.h"

#include <QSplitter>
#include <QHBoxLayout>

#include "UI/sdlwindow.h"

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow();

    ~MainWindow() override;

    bool init_UI();

public slots:
    // void update_window_size();

private:
    QVBoxLayout *main_layout = nullptr;

    QSplitter *main_splitter = nullptr;

    SDLWindow *sdl_window = nullptr;

    // TitleBar *main_title_bar = nullptr;

    // ToolsBar *main_tools_bar = nullptr;

private:
    QPoint drag_position;

    QPoint mouse_press_pos;

    QRect original_geometry;

    int resize_edge = 0;

private:
    void init_layout();

    // void init_conn();

    // void init_CSS();

    // void handle_resize(const QPoint &global_mouse_pos);

    // void update_cursor_shape(const QPoint &pos);

protected:
    // void resizeEvent(QResizeEvent *event) override;

    // void mousePressEvent(QMouseEvent *event) override;

    // void mouseMoveEvent(QMouseEvent *event) override;

    // void mouseReleaseEvent(QMouseEvent *event) override;

    // void paintEvent(QPaintEvent *event) override;
};

#endif //MAINWINDOW_H
