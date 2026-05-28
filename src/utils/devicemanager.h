//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_DEVICEMANAGER_H
#define OBSIM_DEVICEMANAGER_H

#include "PCH.h"
#include <QMediaDevices>
#include <QCameraDevice>
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys.h>

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

struct AudioOutputInfo {
    QString id;          // WASAPI 设备 ID
    QString name;        // 设备友好名称（如 "扬声器 (Realtek Audio)"）
    bool is_default;     // 是否为系统默认播放设备
};

struct AudioInputInfo {
    QString name;        // DShow 设备描述（友好名称，如 "麦克风 (Realtek Audio)"）
    QString raw_name;    // DShow 原始设备名（用于 "audio=XXX" 构造）
    bool is_default;     // 是否为系统默认录音设备
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

    // 获取所有 WASAPI 音频输出设备（系统音频）
    QVector<AudioOutputInfo> get_all_audio_outputs() const;

    // 获取所有 DShow 音频输入设备（麦克风）
    QVector<AudioInputInfo> get_all_audio_inputs() const;

    // 获取默认音频输出设备
    AudioOutputInfo get_default_audio_output() const;

    // 获取默认音频输入设备
    AudioInputInfo get_default_audio_input() const;

private:
    DisplayInfo convert_to_display_info(QScreen *screen, int index, bool is_primary) const;
};


#endif //OBSIM_DEVICEMANAGER_H
