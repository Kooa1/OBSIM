//
// Created by 66 on 2025/8/21.
//

#include "UI/mainwindow.h"

MainWindow::MainWindow() : QWidget(nullptr) {
    setMouseTracking(true);

    init_UI();

    QThread::currentThread()->setPriority(QThread::HighPriority);
}

MainWindow::~MainWindow() = default;

bool MainWindow::init_UI() {
    main_layout = new QVBoxLayout(this);
    main_splitter = new QSplitter(Qt::Vertical, this);
    sdl_window = new SDLWindow(this);
    control_bar = new ControlBar(this);

    init_layout();

    return true;
}

void MainWindow::init_layout() {
    main_splitter->addWidget(sdl_window);
    main_splitter->addWidget(control_bar);
    main_layout->addWidget(main_splitter);

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
