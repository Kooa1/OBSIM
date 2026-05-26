//
// Created by 66 on 2026/4/26.
//

#include "devicemanager.h"

DeviceManager::DeviceManager(QObject *parent) : QObject(parent) {
}

void DeviceManager::run() {
    // 创建 DeviceManager 实例
    DeviceManager device_manager;

    // 获取所有显示器信息
    QVector<DisplayInfo> displays = device_manager.get_all_displays();

    for (const DisplayInfo &info: displays) {
        if (info.is_primary) {
            qDebug() << "主屏幕:" << info.name;
            qDebug() << "  几何区域:" << info.geometry;
            qDebug() << "  刷新率:" << info.refresh_rate << "Hz";
        } else {
            qDebug() << "次屏幕:" << info.name;
            qDebug() << "  几何区域:" << info.geometry;
            qDebug() << "  位置偏移:" << info.geometry.x() << "," << info.geometry.y();
        }
    }

    // 获取主屏幕信息
    DisplayInfo primary = device_manager.get_primary_display();

    // 获取屏幕数量
    int screen_count = device_manager.get_display_count();

    // 获取所有摄像头信息
    QVector<CameraInfo> cameras = device_manager.get_all_cameras();

    for (const CameraInfo &cam: cameras) {
        qDebug() << "摄像头:" << cam.index;
        qDebug() << "  名称:" << cam.name.c_str();
        qDebug() << "  默认设备:" << (cam.is_default ? "是" : "否");
    }

    // 获取摄像头数量
    int camera_count = device_manager.get_camera_count();
}

QVector<DisplayInfo> DeviceManager::get_all_displays() const {
    QVector<DisplayInfo> displays;
    QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *primary_screen = QGuiApplication::primaryScreen();

    for (int i = 0; i < screens.size(); ++i) {
        bool is_primary = (screens[i] == primary_screen);
        displays.append(convert_to_display_info(screens[i], i, is_primary));
    }

    return displays;
}

QVector<CameraInfo> DeviceManager::get_all_cameras() const {
    QVector<CameraInfo> cameras;
    QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    QByteArray default_id = QMediaDevices::defaultVideoInput().id();

    for (int i = 0; i < devices.size(); ++i) {
        CameraInfo info;
        info.index = i;
        info.name = devices[i].description().toStdString();
        info.id = devices[i].id();
        info.is_default = (devices[i].id() == default_id);
        cameras.append(info);
    }

    return cameras;
}

int DeviceManager::get_display_count() const {
    return QGuiApplication::screens().size();
}

int DeviceManager::get_camera_count() const {
    return QMediaDevices::videoInputs().size();
}

DisplayInfo DeviceManager::get_primary_display() const {
    QScreen *primary_screen = QGuiApplication::primaryScreen();
    QList<QScreen *> screens = QGuiApplication::screens();

    for (int i = 0; i < screens.size(); ++i) {
        if (screens[i] == primary_screen) {
            return convert_to_display_info(primary_screen, i, true);
        }
    }

    return DisplayInfo{}; // 返回空结构体
}

DisplayInfo DeviceManager::get_display(int index) const {
    QList<QScreen *> screens = QGuiApplication::screens();
    if (index < 0 || index >= screens.size()) {
        return DisplayInfo{};
    }

    QScreen *screen = screens[index];
    QScreen *primary_screen = QGuiApplication::primaryScreen();
    bool is_primary = (screen == primary_screen);

    return convert_to_display_info(screen, index, is_primary);
}

CameraInfo DeviceManager::get_camera(int index) const {
    QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    if (index < 0 || index >= devices.size()) {
        return CameraInfo{};
    }

    CameraInfo info;
    info.index = index;
    info.name = devices[index].description().toStdString();
    info.id = devices[index].id();
    info.is_default = (devices[index].id() == QMediaDevices::defaultVideoInput().id());

    return info;
}

bool DeviceManager::is_primary_screen(int index) const {
    QList<QScreen *> screens = QGuiApplication::screens();
    if (index < 0 || index >= screens.size()) {
        return false;
    }
    return (screens[index] == QGuiApplication::primaryScreen());
}

DisplayInfo DeviceManager::convert_to_display_info(QScreen *screen, int index, bool is_primary) const {
    DisplayInfo info;
    info.index = index;
    info.name = screen->name().toStdString();
    info.manufacturer = screen->manufacturer().toStdString();
    info.model = screen->model().toStdString();
    info.geometry = screen->geometry();
    info.available_geometry = screen->availableGeometry();
    info.is_primary = is_primary;
    info.dpi = screen->physicalDotsPerInch();
    info.refresh_rate = screen->refreshRate();

    return info;
}
