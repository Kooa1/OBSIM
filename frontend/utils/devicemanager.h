//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_DEVICEMANAGER_H
#define OBSIM_DEVICEMANAGER_H

#include "PCH.h"

struct DisplayInfo {
    int index; // 屏幕索引
    std::string name; // 屏幕名称
    std::string manufacturer; // 制造商
    std::string model; // 型号
    QRect geometry; // 屏幕几何区域 (位置和大小)
    QRect available_geometry; // 可用区域 (排除任务栏等)
    bool is_primary; // 是否为主屏幕
    qreal dpi; // DPI
    qreal refresh_rate; // 刷新率
};

struct CameraInfo {
    int index;           // 摄像头索引
    std::string name;    // 摄像头名称/描述
    QByteArray id;       // 摄像头唯一标识符
    bool is_default;     // 是否为系统默认摄像头
};

class DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    // 静态方法：直接运行示例
    static void run();

    // 获取所有显示器信息
    QVector<DisplayInfo> get_all_displays() const;

    // 获取所有摄像头信息
    QVector<CameraInfo> get_all_cameras() const;

    // 获取显示器数量
    int get_display_count() const;

    // 获取摄像头数量
    int get_camera_count() const;

    // 获取主显示器
    DisplayInfo get_primary_display() const;

    // 根据索引获取显示器
    DisplayInfo get_display(int index) const;

    // 根据索引获取摄像头
    CameraInfo get_camera(int index) const;

    // 判断指定屏幕是否为主屏
    bool is_primary_screen(int index) const;

private:
    DisplayInfo convert_to_display_info(QScreen *screen, int index, bool is_primary) const;
};


#endif //OBSIM_DEVICEMANAGER_H
