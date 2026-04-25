//
// Created by 66 on 2025/8/21.
//

#include "UI/mainwindow.h"

MainWindow::MainWindow() : QWidget(nullptr) {
    // this->setWindowFlags(Qt::FramelessWindowHint);
    //    this->setMinimumSize(640, 480);
    setMouseTracking(true);

    init_UI();

    QThread::currentThread()->setPriority(QThread::HighPriority);
}

MainWindow::~MainWindow() = default;

bool MainWindow::init_UI() {
    main_layout = new QVBoxLayout(this);
    main_splitter = new QSplitter(Qt::Horizontal, this);
    sdl_window = new SDLWindow(this);
    // main_title_bar = new TitleBar(this);
    // main_tools_bar = new ToolsBar(this);

    init_layout();

    return true;
}

// void MainWindow::update_window_size() {
//     // 保存当前窗口中心点
//     QRect oldGeometry = this->geometry();
//     QPoint center = oldGeometry.center();
//
//     // 调整窗口大小
//     this->adjustSize();
//
//     // 计算新位置（保持原中心点）
//     int x = center.x() - this->width() / 2;
//     int y = center.y() - this->height() / 2;
//
//     // 确保窗口在屏幕可见区域内
//     QScreen *screen = QGuiApplication::screenAt(center);
//     if (!screen) {
//         screen = QGuiApplication::primaryScreen();
//     }
//     QRect screenGeometry = screen->availableGeometry();
//
//     x = qMax(screenGeometry.x(), qMin(x, screenGeometry.x() + screenGeometry.width() - this->width()));
//     y = qMax(screenGeometry.y(), qMin(y, screenGeometry.y() + screenGeometry.height() - this->height()));
//
//     // 移动窗口到新位置
//     this->move(x, y);
// }

void MainWindow::init_layout() {
    //test
    main_splitter->addWidget(sdl_window);

    // main_layout->addWidget(main_title_bar);
    main_layout->addWidget(main_splitter);
    // main_layout->addWidget(main_tools_bar);

    // init_conn();

    this->show();
}

// void MainWindow::init_conn() {
//     connect(main_tools_bar->start_obj(), &QToolButton::clicked, this, [this]() {
//         if (sdl_window->get_sdl_initialized_symbol()) {
//             if (!sdl_window->get_rendering_symbol()) {
//                 if (sdl_window->open_file()) {
//                     sdl_window->start_rendering();
//                     main_tools_bar->set_play_icon(true);
//                     main_tools_bar->set_duration_disable(false);
//                 }
//             } else {
//                 if (!sdl_window->get_rendering_pause_symbol()) {
//                     sdl_window->set_rendering_pause_symbol(true);
//                     main_tools_bar->set_play_icon(false);
//                     main_tools_bar->set_duration_disable(true);
//                 } else {
//                     sdl_window->set_rendering_pause_symbol(false);
//                     main_tools_bar->set_play_icon(true);
//                     main_tools_bar->set_duration_disable(false);
//                 }
//             }
//         }
//     }, Qt::QueuedConnection);
//
//     connect(main_tools_bar->stop_obj(), &QToolButton::clicked, this, [this]() {
//         if (sdl_window->get_rendering_symbol()) {
//             SDL_PauseAudio(-1);
//             sdl_window->stop_stream_line();
//             main_tools_bar->set_play_icon(false);
//             main_tools_bar->clean_duration();
//             sdl_window->stop_rendering();
//             sdl_window->shutdown_audio_device();
//             main_tools_bar->set_duration_disable(true);
//         }
//     }, Qt::DirectConnection);
//
//     connect(main_tools_bar->open_file_obj(), &QToolButton::clicked, this, [this]() {
//         if (sdl_window->get_rendering_symbol()) {
//             SDL_PauseAudio(-1);
//             sdl_window->stop_stream_line();
//             main_tools_bar->set_play_icon(false);
//             main_tools_bar->clean_duration();
//             sdl_window->stop_rendering();
//             sdl_window->shutdown_audio_device();
//             main_tools_bar->set_duration_disable(true);
//
//             const auto file_name = sdl_window->open_file_dialog();
//             sdl_window->open_file(file_name);
//             main_tools_bar->set_duration_disable(false);
//         } else {
//             if (sdl_window->open_file()) {
//                 sdl_window->start_rendering();
//                 main_tools_bar->set_play_icon(true);
//                 main_tools_bar->set_duration_disable(false);
//             }
//         }
//     }, Qt::DirectConnection);
//
//     connect(sdl_window, &SDLWindow::size_hint_change, this, &MainWindow::update_window_size,
//             Qt::QueuedConnection);
//
//     connect(sdl_window, &SDLWindow::whole_video_duration, main_tools_bar, &ToolsBar::change_tools_bar_duration,
//             Qt::DirectConnection);
//
//     connect(sdl_window, &SDLWindow::play_over, this, [this]() {
//         cout << "play over\n";
//         SDL_PauseAudio(-1);
//         sdl_window->stop_stream_line();
//         sdl_window->stop_rendering();
//         main_tools_bar->set_play_icon(false);
//         main_tools_bar->set_duration_disable(true);
//         main_tools_bar->clean_duration();
//         sdl_window->shutdown_audio_device();
//     });
//
//     connect(sdl_window, &SDLWindow::add_second, this, [this]() {
//         main_tools_bar->add_second();
//     }, Qt::QueuedConnection);
//
//     connect(main_tools_bar, &ToolsBar::seek, this, [this](const int current_second) {
//         if (current_second == -1) {
//             sdl_window->play_over();
//             return;
//         }
//         sdl_window->set_seek_symbol(true, current_second);
//     });
//
//     connect(main_tools_bar, &ToolsBar::volume, this, [this](const int volume) {
//         sdl_window->set_volume(volume);
//     });
//
//     connect(main_tools_bar, &ToolsBar::silent, this, [this]() {
//         sdl_window->silent_volume();
//         main_tools_bar->set_volume_icon(false);
//     });
//
//     connect(main_tools_bar, &ToolsBar::s_restore, this, [this]() {
//         sdl_window->restore_volume();
//         main_tools_bar->set_volume_icon(true);
//     });
// }

// void MainWindow::init_CSS() {
    //    main_splitter->setStyleSheet(R"(
    //        QSplitter::handle {
    //            background-color: #c0c0c0;
    //            border: 1px solid #a0a0a0;
    //        }
    //        QSplitter::handle:hover {
    //            background-color: #a0a0a0;
    //        }
    //        QSplitter::handle:horizontal {
    //            width: 12px;
    //            image: none;
    //        }
    //    )");

    //    this->setStyleSheet(
    //            "QMainWindow {"
    //            "   background-color: #3498db;"
    //            "   border: 3px solid #2980b9;"
    //            "   border-radius: 5px;"
    //            "}"
    //            "QLabel {"
    //            "   color: white;"
    //            "   font-size: 16px;"
    //            "}"
    //    );
// }

// 处理调整大小
// void MainWindow::handle_resize(const QPoint &global_mouse_pos) {
//     QRect new_geometry = original_geometry;
//     const QPoint delta = global_mouse_pos - mouse_press_pos;
//
//     if (resize_edge & Qt::LeftEdge) {
//         new_geometry.setLeft(original_geometry.left() + delta.x());
//         if (new_geometry.width() < minimumWidth()) {
//             new_geometry.setLeft(original_geometry.right() - minimumWidth());
//         }
//     }
//     if (resize_edge & Qt::RightEdge) {
//         new_geometry.setRight(original_geometry.right() + delta.x());
//         if (new_geometry.width() < minimumWidth()) {
//             new_geometry.setRight(original_geometry.left() + minimumWidth());
//         }
//     }
//     if (resize_edge & Qt::TopEdge) {
//         new_geometry.setTop(original_geometry.top() + delta.y());
//         if (new_geometry.height() < minimumHeight()) {
//             new_geometry.setTop(original_geometry.bottom() - minimumHeight());
//         }
//     }
//     if (resize_edge & Qt::BottomEdge) {
//         new_geometry.setBottom(original_geometry.bottom() + delta.y());
//         if (new_geometry.height() < minimumHeight()) {
//             new_geometry.setBottom(original_geometry.top() + minimumHeight());
//         }
//     }
//
//     setGeometry(new_geometry);
// }

// 更新光标形状
// void MainWindow::update_cursor_shape(const QPoint &pos) {
//     if (pos.x() < 5 && pos.y() < 5) {
//         this->setCursor(Qt::SizeFDiagCursor);
//     } else if (pos.x() < 5 && pos.y() > height() - 5) {
//         this->setCursor(Qt::SizeBDiagCursor);
//     } else if (pos.x() > width() - 5 && pos.y() < 5) {
//         this->setCursor(Qt::SizeBDiagCursor);
//     } else if (pos.x() > width() - 5 && pos.y() > height() - 5) {
//         this->setCursor(Qt::SizeFDiagCursor);
//     } else if (pos.x() < 5) {
//         this->setCursor(Qt::SizeHorCursor);
//     } else if (pos.x() > width() - 5) {
//         this->setCursor(Qt::SizeHorCursor);
//     } else if (pos.y() < 5) {
//         this->setCursor(Qt::SizeVerCursor);
//     } else if (pos.y() > height() - 5) {
//         this->setCursor(Qt::SizeVerCursor);
//     } else {
//         this->setCursor(Qt::ArrowCursor);
//     }
// }

// void MainWindow::resizeEvent(QResizeEvent *event) {
//     QWidget::resizeEvent(event);
// }

// 鼠标按下事件 - 用于拖动和调整大小
// void MainWindow::mousePressEvent(QMouseEvent *event) {
//     if (event->button() == Qt::LeftButton) {
//         drag_position = event->globalPosition().toPoint() - frameGeometry().topLeft();
//         mouse_press_pos = event->globalPosition().toPoint();
//         original_geometry = geometry();
//
//         // 确定鼠标在哪个边缘（用于调整大小）
//         QPoint pos = event->pos();
//         resize_edge = 0;
//
//         if (pos.x() < 5) resize_edge |= Qt::LeftEdge;
//         if (pos.x() > width() - 5) resize_edge |= Qt::RightEdge;
//         if (pos.y() < 5) resize_edge |= Qt::TopEdge;
//         if (pos.y() > height() - 5) resize_edge |= Qt::BottomEdge;
//
//         event->accept();
//     }
// }

// 鼠标移动事件 - 用于拖动和调整大小
// void MainWindow::mouseMoveEvent(QMouseEvent *event) {
//     if (event->buttons() & Qt::LeftButton) {
//         if (resize_edge != 0) {
//             // 调整大小
//             handle_resize(event->globalPosition().toPoint());
//         } else {
//             // 拖动窗口
//             move(event->globalPosition().toPoint() - drag_position);
//         }
//         event->accept();
//     } else {
//         // 更新光标形状
//         update_cursor_shape(event->pos());
//     }
// }

// 鼠标释放事件
// void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
//     if (event->button() == Qt::LeftButton) {
//         resize_edge = 0;
//         setCursor(Qt::ArrowCursor);
//         event->accept();
//     }
// }

// 绘制事件 - 添加一些视觉效果
// void MainWindow::paintEvent(QPaintEvent *event) {
//     Q_UNUSED(event);
//     QPainter painter(this);
//     painter.setRenderHint(QPainter::Antialiasing);
//
//     // 创建阴影渐变
//     QRect shadow_rect = rect().adjusted(3, 3, -3, -3);
//     QLinearGradient gradient(shadow_rect.topLeft(), shadow_rect.bottomRight());
//     gradient.setColorAt(0, QColor(0, 0, 0, 2));
//     gradient.setColorAt(1, Qt::transparent);
//
//     // 绘制阴影
//     painter.setPen(Qt::NoPen);
//     painter.setBrush(gradient);
//     painter.drawRect(shadow_rect);
//
//     // 绘制边框
//     painter.setPen(QPen(QColor(0, 0, 0), 2));
//     painter.drawRect(rect().adjusted(1, 1, -1, -1));
// }
