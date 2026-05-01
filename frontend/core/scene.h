//
// Created by 66 on 2026/4/30.
//

#ifndef OBSIM_SCENE_H
#define OBSIM_SCENE_H

#include <Windows.h>

#include <vector>
#include <algorithm>
#include <cmath>

#include <GL/gl.h>

#include <QPointF>
#include <QRectF>

#include "core/source.h"

// 交互模式
enum class InteractionMode {
    None, // 无操作
    Hover, // 悬停（用于后续高亮）
    Dragging, // 拖动源
    Resizing, // 缩放源（暂不实现，预留）
    Selecting // 框选
};

// 缩放的控点位置
enum class ResizeHandle {
    None,
    TopLeft, Top, TopRight,
    Left, Right,
    BottomLeft, Bottom, BottomRight
};

class Scene {
public:
    explicit Scene();

    // ==================== 源管理 ====================

    // 获取所有源（用于渲染遍历，从前到后是底层到顶层）
    std::vector<Source *> &get_sources() { return sources; }
    const std::vector<Source *> &get_sources() const { return sources; }

    void add_source(Source *source);

    void remove_source(Source *source);

    // 将源移到顶层/底层
    void move_to_top(Source *source);

    void move_to_bottom(Source *source);

    void move_up(Source *source);

    void move_down(Source *source);

    // ==================== 选中状态 ====================

    // 当前选中的源（单选，后续可扩展为多选）
    Source *selected_source() const { return m_selected_source; }

    void set_selected_source(Source *source);

    void clear_selection();

    // ==================== 命中测试 ====================

    // 从顶层到底层遍历，返回第一个命中的源
    Source *hit_test(const QPointF &canvas_pos) const;

    // ==================== 交互状态机 ====================

    // 鼠标按下时调用，返回 true 表示事件被处理
    // canvas_pos: 画布逻辑坐标
    bool on_mouse_press(const QPointF &canvas_pos);

    // 鼠标移动时调用，返回 true 表示需要重绘
    bool on_mouse_move(const QPointF &canvas_pos);

    // 鼠标释放时调用
    void on_mouse_release();

    // 当前交互模式
    InteractionMode interaction_mode() const { return m_mode; }

    // 获取当前鼠标在画布上的位置
    QPointF mouse_canvas_pos() const { return m_mouse_pos; }

    // ==================== 选中框绘制（在 paintGL 最后调用） ====================

    // 绘制选中源的边框和控点
    // 需在正交投影已设置好、glLoadIdentity 后的状态下调用
    void render_selection_box();

    // 设置吸附阈值（画布逻辑像素）
    void set_snap_threshold(float threshold) { m_snap_threshold = threshold; }
    float snap_threshold() const { return m_snap_threshold; }

    // 是否启用吸附
    void set_snap_enabled(bool enabled) { m_snap_enabled = enabled; }
    bool is_snap_enabled() const { return m_snap_enabled; }

    // 获取当前吸附线（用于渲染辅助线）
    struct SnapLine {
        enum Orientation { Horizontal, Vertical };

        Orientation orientation;
        float position; // 画布坐标位置
    };

    // 绘制吸附辅助线（在 render_selection_box 中或单独调用）
    void render_snap_lines();

private:
    // 获取源上鼠标所在的控点
    ResizeHandle get_handle_at_pos(Source *source, const QPointF &canvas_pos) const;

    // 控点对应的光标形状
    Qt::CursorShape cursor_for_handle(ResizeHandle handle) const;

    // 计算吸附位置
    // 返回修正后的位置
    QPointF snap_position(const QPointF &proposed_pos,
                          float width, float height,
                          Source *exclude_source = nullptr);

    // 收集所有吸附参考点
    // 画布边界 + 其他源的边和中心线
    struct SnapTarget {
        float value;

        enum Type { CanvasEdge, SourceEdge, SourceCenter };

        Type type;
        Source *source; // 如果是源产生的目标，记录来源
    };

private:
    // 源列表（裸指针，生命周期由外部 unique_ptr 管理）
    std::vector<Source *> sources;

    // 当前选中的源
    Source *m_selected_source = nullptr;

    // 交互状态
    InteractionMode m_mode = InteractionMode::None;

    // 鼠标在画布上的当前位置
    QPointF m_mouse_pos;

    // 拖动/缩放时的起始数据
    QPointF m_drag_start_mouse; // 鼠标按下时的画布坐标
    float m_drag_start_pos_x = 0; // 源按下时的 pos_x
    float m_drag_start_pos_y = 0; // 源按下时的 pos_y
    float m_drag_start_width = 0; // 源按下时的 base_width（缩放用）
    float m_drag_start_height = 0; // 源按下时的 base_height（缩放用）
    ResizeHandle m_active_handle = ResizeHandle::None; // 当前操作的控点

    std::vector<SnapLine> current_snap_lines() const { return m_current_snap_lines; }

    void collect_snap_targets(Source* exclude_source,
                          std::vector<SnapTarget>& out_vertical,
                          std::vector<SnapTarget>& out_horizontal);

    // 吸附阈值（画布逻辑像素，默认10）
    float m_snap_threshold = 10.0f;
    bool m_snap_enabled = true;

    // 当前帧的吸附线（用于渲染）
    std::vector<SnapLine> m_current_snap_lines;
};


#endif //OBSIM_SCENE_H
