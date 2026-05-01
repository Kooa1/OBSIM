//
// Created by 66 on 2026/4/29.
//

#include "source.h"

Source::Source() {
    // 生成一个简单的唯一 ID（实际项目可用 UUID 或自增计数器）
    static int next_id = 0;
    id = "source_" + std::to_string(next_id++);
}

Source::~Source() {
}

QRectF Source::get_bounding_rect() const {
    // 计算有效渲染区域（考虑裁剪）
    QRectF content = get_content_rect();

    // 应用缩放
    float render_w = content.width() * scale_x;
    float render_h = content.height() * scale_y;

    // 当前版本暂不考虑旋转对包围盒的影响，只返回轴对齐矩形
    // 如果 rotation != 0，包围盒会更大，这里简化处理
    return QRectF(pos_x, pos_y, render_w, render_h);
}

QRectF Source::get_content_rect() const {
    float left = crop_left;
    float top = crop_top;
    float right = base_width - crop_right;
    float bottom = base_height - crop_bottom;

    if (left >= right || top >= bottom) {
        return QRectF(0, 0, base_width, base_height);
    }

    return QRectF(left, top, right - left, bottom - top);
}

bool Source::hit_test(const QPointF &canvas_pos) const {
    if (!visible) return false;

    QRectF bounds = get_bounding_rect();
    return bounds.contains(canvas_pos);
}

QMatrix4x4 Source::get_local_to_canvas_transform() const {
    QMatrix4x4 matrix;
    matrix.setToIdentity();

    // 1. 移动到画布目标位置
    matrix.translate(pos_x, pos_y, 0);

    // 2. 如果非零旋转，围绕内容中心旋转
    if (std::abs(rotation) > 0.001f) {
        float render_w = get_render_width();
        float render_h = get_render_height();
        matrix.translate(render_w / 2.0f, render_h / 2.0f, 0);
        matrix.rotate(rotation, 0, 0, 1);
        matrix.translate(-render_w / 2.0f, -render_h / 2.0f, 0);
    }

    // 3. 应用缩放
    matrix.scale(scale_x, scale_y, 1.0f);

    return matrix;
}

float Source::get_render_width() const {
    QRectF content = get_content_rect();
    return content.width();
}

float Source::get_render_height() const {
    QRectF content = get_content_rect();
    return content.height();
}

QPointF Source::canvas_to_local(const QPointF &canvas_pos) const {
    // 简化版：将画布坐标转为本地坐标
    // 精确版本应用 get_local_to_canvas_transform().inverted()
    float local_x = (canvas_pos.x() - pos_x) / scale_x + crop_left;
    float local_y = (canvas_pos.y() - pos_y) / scale_y + crop_top;
    return QPointF(local_x, local_y);
}
