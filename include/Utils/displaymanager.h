//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_DISPLAYMANAGER_H
#define OBSIM_DISPLAYMANAGER_H

#include "PCH.h"

struct DisplayInfo {
    int index; // 屏幕索引
    QString name; // 屏幕名称
    QString manufacturer; // 制造商
    QString model; // 型号
    QRect geometry; // 屏幕几何区域 (位置和大小)
    QRect available_geometry; // 可用区域 (排除任务栏等)
    bool is_primary; // 是否为主屏幕
    qreal dpi; // DPI
    qreal refresh_rate; // 刷新率
};

class DisplayManager : public QObject {
    Q_OBJECT

public:
    explicit DisplayManager(QObject *parent = nullptr);

    // 获取所有显示器信息
    QVector<DisplayInfo> get_all_displays() const;

    // 获取显示器数量
    int get_display_count() const;

    // 获取主显示器
    DisplayInfo get_primary_display() const;

    // 根据索引获取显示器
    DisplayInfo get_display(int index) const;

    // 判断指定屏幕是否为主屏
    bool is_primary_screen(int index) const;

    void run();

private:
    DisplayInfo convert_to_display_info(QScreen *screen, int index, bool is_primary) const;
};


#endif //OBSIM_DISPLAYMANAGER_H
