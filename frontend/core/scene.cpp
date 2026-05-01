//
// Created by 66 on 2026/4/30.
//
// scene.cpp
#include "scene.h"


// ==================== 源管理 ====================

Scene::Scene() {
    m_snap_threshold = 25.0f; // 25 个逻辑像素内触发吸附
    m_snap_enabled = true; // 默认开启
}

void Scene::add_source(Source *source) {
    if (!source) return;
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it == sources.end()) {
        sources.push_back(source);
    }
}

void Scene::remove_source(Source *source) {
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it != sources.end()) {
        sources.erase(it);
    }
    if (m_selected_source == source) {
        m_selected_source = nullptr;
    }
}

void Scene::move_to_top(Source *source) {
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it != sources.end()) {
        sources.erase(it);
        sources.push_back(source);
    }
}

void Scene::move_to_bottom(Source *source) {
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it != sources.end()) {
        sources.erase(it);
        sources.insert(sources.begin(), source);
    }
}

void Scene::move_up(Source *source) {
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it != sources.end() && it + 1 != sources.end()) {
        std::iter_swap(it, it + 1);
    }
}

void Scene::move_down(Source *source) {
    auto it = std::find(sources.begin(), sources.end(), source);
    if (it != sources.end() && it != sources.begin()) {
        std::iter_swap(it, it - 1);
    }
}

// ==================== 选中状态 ====================

void Scene::set_selected_source(Source *source) {
    // 取消旧选中的标记
    if (m_selected_source) {
        m_selected_source->selected = false;
    }
    m_selected_source = source;
    if (m_selected_source) {
        m_selected_source->selected = true;
    }
}

void Scene::clear_selection() {
    set_selected_source(nullptr);
}

// ==================== 命中测试 ====================

Source *Scene::hit_test(const QPointF &canvas_pos) const {
    // 从顶层向底层遍历
    for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
        Source *source = *it;
        if (source->visible && source->hit_test(canvas_pos)) {
            return source;
        }
    }
    return nullptr;
}

// ==================== 交互状态机 ====================

bool Scene::on_mouse_press(const QPointF &canvas_pos) {
    m_mouse_pos = canvas_pos;
    Source *hit = hit_test(canvas_pos);

    if (hit) {
        set_selected_source(hit);
        ResizeHandle handle = get_handle_at_pos(hit, canvas_pos);

        if (handle != ResizeHandle::None) {
            m_mode = InteractionMode::Resizing;
            m_active_handle = handle;
        } else {
            m_mode = InteractionMode::Dragging;
            m_active_handle = ResizeHandle::None;
        }

        m_drag_start_mouse = canvas_pos;
        m_drag_start_pos_x = hit->pos_x;
        m_drag_start_pos_y = hit->pos_y;
        m_drag_start_width = hit->base_width;
        m_drag_start_height = hit->base_height;
        m_drag_start_scale_x = hit->scale_x; // ✅ 记录
        m_drag_start_scale_y = hit->scale_y; // ✅ 记录

        return true;
    } else {
        clear_selection();
        m_mode = InteractionMode::None;
        return false;
    }
}

bool Scene::on_mouse_move(const QPointF &canvas_pos) {
    m_mouse_pos = canvas_pos;
    m_current_snap_lines.clear();

    // 🖱️ 悬停检测（非拖动/缩放模式时）
    if (m_mode == InteractionMode::None) {
        Source* hit = hit_test(canvas_pos);
        if (hit != m_hovered_source) {
            m_hovered_source = hit;
            return true;  // 需要重绘
        }
    } else {
        m_hovered_source = nullptr;
    }

    if (m_mode == InteractionMode::Dragging && m_selected_source) {
        float dx = canvas_pos.x() - m_drag_start_mouse.x();
        float dy = canvas_pos.y() - m_drag_start_mouse.y();

        float new_x = m_drag_start_pos_x + dx;
        float new_y = m_drag_start_pos_y + dy;

        QPointF snapped = snap_position(
            QPointF(new_x, new_y),
            m_selected_source->get_bounding_rect().width(),
            m_selected_source->get_bounding_rect().height(),
            m_selected_source
        );

        m_selected_source->pos_x = snapped.x();
        m_selected_source->pos_y = snapped.y();

        return true;
    }

    if (m_mode == InteractionMode::Resizing && m_selected_source) {
        float dx = canvas_pos.x() - m_drag_start_mouse.x();
        float dy = canvas_pos.y() - m_drag_start_mouse.y();

        // ✅ 基于拖拽前的尺寸计算新的 scale
        float start_w = m_drag_start_width * m_drag_start_scale_x;
        float start_h = m_drag_start_height * m_drag_start_scale_y;
        float aspect = m_drag_start_width / m_drag_start_height;

        float new_w = start_w;
        float new_h = start_h;
        float new_x = m_drag_start_pos_x;
        float new_y = m_drag_start_pos_y;

        switch (m_active_handle) {
            case ResizeHandle::Right:
                new_w = std::max(10.0f, start_w + dx);
                new_h = new_w / aspect;
                break;

            case ResizeHandle::Bottom:
                new_h = std::max(10.0f, start_h + dy);
                new_w = new_h * aspect;
                break;

            case ResizeHandle::BottomRight: {
                float s = std::max((start_w + dx) / start_w, (start_h + dy) / start_h);
                new_w = std::max(10.0f, start_w * s);
                new_h = new_w / aspect;
                break;
            }

            case ResizeHandle::Left:
                new_w = std::max(10.0f, start_w - dx);
                new_h = new_w / aspect;
                new_x = m_drag_start_pos_x + (start_w - new_w);
                break;

            case ResizeHandle::Top:
                new_h = std::max(10.0f, start_h - dy);
                new_w = new_h * aspect;
                new_y = m_drag_start_pos_y + (start_h - new_h);
                break;

            case ResizeHandle::TopLeft: {
                float s = std::max((start_w - dx) / start_w, (start_h - dy) / start_h);
                new_w = std::max(10.0f, start_w * s);
                new_h = new_w / aspect;
                new_x = m_drag_start_pos_x + (start_w - new_w);
                new_y = m_drag_start_pos_y + (start_h - new_h);
                break;
            }

            case ResizeHandle::TopRight: {
                float s = std::max((start_w + dx) / start_w, (start_h - dy) / start_h);
                new_w = std::max(10.0f, start_w * s);
                new_h = new_w / aspect;
                new_y = m_drag_start_pos_y + (start_h - new_h);
                break;
            }

            case ResizeHandle::BottomLeft: {
                float s = std::max((start_w - dx) / start_w, (start_h + dy) / start_h);
                new_w = std::max(10.0f, start_w * s);
                new_h = new_w / aspect;
                new_x = m_drag_start_pos_x + (start_w - new_w);
                break;
            }

            default:
                break;
        }

        // ✅ 只修改 scale，不动 base
        m_selected_source->pos_x = new_x;
        m_selected_source->pos_y = new_y;
        m_selected_source->scale_x = new_w / m_drag_start_width;
        m_selected_source->scale_y = new_h / m_drag_start_height;

        // 🧲 检查满屏吸附
        snap_to_fullscreen(m_selected_source);

        return true;
    }

    return false;
}

void Scene::on_mouse_release() {
    m_mode = InteractionMode::None;
    m_active_handle = ResizeHandle::None;
    m_current_snap_lines.clear(); // ← 加这一行
}

// ==================== 选中框绘制 ====================

void Scene::render_selection_box() {
    render_hover_box();

    if (!m_selected_source || !m_selected_source->visible) return;

    Source *src = m_selected_source;
    QRectF bounds = src->get_bounding_rect();

    float x = bounds.x();
    float y = bounds.y();
    float w = bounds.width();
    float h = bounds.height();

    // 绘制白色边框
    glLineWidth(3.0f);
    glColor4f(1.0f, 0.2f, 0.2f, 1.0f);

    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();

    // 绘制8个控点（小方块）
    const float handle_size = 8.0f;
    const float half = handle_size / 2.0f;

    // 控点中心位置
    struct {
        float cx, cy;
    } handles[] = {
                {x, y}, // TopLeft
                {x + w / 2, y}, // Top
                {x + w, y}, // TopRight
                {x, y + h / 2}, // Left
                {x + w, y + h / 2}, // Right
                {x, y + h}, // BottomLeft
                {x + w / 2, y + h}, // Bottom
                {x + w, y + h} // BottomRight
            };

    // 先画填充的蓝色方块
    glColor4f(0.2f, 0.5f, 1.0f, 1.0f);
    for (const auto &hdl: handles) {
        glBegin(GL_QUADS);
        glVertex2f(hdl.cx - half, hdl.cy - half);
        glVertex2f(hdl.cx + half, hdl.cy - half);
        glVertex2f(hdl.cx + half, hdl.cy + half);
        glVertex2f(hdl.cx - half, hdl.cy + half);
        glEnd();
    }

    // 再画白色边框
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (const auto &hdl: handles) {
        glBegin(GL_LINE_LOOP);
        glVertex2f(hdl.cx - half, hdl.cy - half);
        glVertex2f(hdl.cx + half, hdl.cy - half);
        glVertex2f(hdl.cx + half, hdl.cy + half);
        glVertex2f(hdl.cx - half, hdl.cy + half);
        glEnd();
    }

    glLineWidth(1.0f);
}

// ==================== 辅助函数 ====================

void Scene::render_snap_lines() {
    if (m_current_snap_lines.empty()) return;

    glLineWidth(1.0f);
    glColor4f(1.0f, 0.0f, 0.0f, 0.8f); // 半透明红色辅助线

    for (const auto &line: m_current_snap_lines) {
        glBegin(GL_LINES);
        if (line.orientation == SnapLine::Vertical) {
            glVertex2f(line.position, 0.0f);
            glVertex2f(line.position, 1080.0f); // 画布高度
        } else {
            glVertex2f(0.0f, line.position);
            glVertex2f(1920.0f, line.position); // 画布宽度
        }
        glEnd();
    }

    glLineWidth(1.0f);
}

ResizeHandle Scene::get_handle_at_pos(Source *source, const QPointF &canvas_pos) const {
    if (!source) return ResizeHandle::None;

    QRectF bounds = source->get_bounding_rect();
    const float handle_size = 16.0f; // 点击容差范围
    const float half = handle_size / 2.0f;

    float x = bounds.x();
    float y = bounds.y();
    float w = bounds.width();
    float h = bounds.height();

    // 检测8个控点区域
    struct {
        QRectF rect;
        ResizeHandle handle;
    } regions[] = {
                {QRectF(x - half, y - half, handle_size, handle_size), ResizeHandle::TopLeft},
                {QRectF(x + w / 2 - half, y - half, handle_size, handle_size), ResizeHandle::Top},
                {QRectF(x + w - half, y - half, handle_size, handle_size), ResizeHandle::TopRight},
                {QRectF(x - half, y + h / 2 - half, handle_size, handle_size), ResizeHandle::Left},
                {QRectF(x + w - half, y + h / 2 - half, handle_size, handle_size), ResizeHandle::Right},
                {QRectF(x - half, y + h - half, handle_size, handle_size), ResizeHandle::BottomLeft},
                {QRectF(x + w / 2 - half, y + h - half, handle_size, handle_size), ResizeHandle::Bottom},
                {QRectF(x + w - half, y + h - half, handle_size, handle_size), ResizeHandle::BottomRight},
            };

    for (const auto &region: regions) {
        if (region.rect.contains(canvas_pos)) {
            return region.handle;
        }
    }

    return ResizeHandle::None;
}

Qt::CursorShape Scene::cursor_for_handle(ResizeHandle handle) const {
    switch (handle) {
        case ResizeHandle::TopLeft: return Qt::SizeFDiagCursor;
        case ResizeHandle::TopRight: return Qt::SizeBDiagCursor;
        case ResizeHandle::BottomLeft: return Qt::SizeBDiagCursor;
        case ResizeHandle::BottomRight: return Qt::SizeFDiagCursor;
        case ResizeHandle::Top:
        case ResizeHandle::Bottom: return Qt::SizeVerCursor;
        case ResizeHandle::Left:
        case ResizeHandle::Right: return Qt::SizeHorCursor;
        default: return Qt::ArrowCursor;
    }
}

QPointF Scene::snap_position(const QPointF &proposed_pos,
                             float width, float height,
                             Source *exclude_source) {
    if (!m_snap_enabled) return proposed_pos;

    m_current_snap_lines.clear();

    std::vector<SnapTarget> vert_targets;
    std::vector<SnapTarget> horz_targets;
    collect_snap_targets(exclude_source, vert_targets, horz_targets);

    float final_x = proposed_pos.x();
    float final_y = proposed_pos.y();

    const float left = proposed_pos.x();
    const float right = proposed_pos.x() + width;
    const float top = proposed_pos.y();
    const float bottom = proposed_pos.y() + height;

    // ---- X 轴吸附（只对齐左右边缘） ----
    float best_dist_x = m_snap_threshold + 1.0f;
    SnapLine best_line_x;
    bool has_x = false;

    for (const auto &t: vert_targets) {
        float dl = std::abs(left - t.value);
        if (dl < best_dist_x) {
            best_dist_x = dl;
            final_x = t.value;
            best_line_x = {SnapLine::Vertical, t.value};
            has_x = true;
        }

        float dr = std::abs(right - t.value);
        if (dr < best_dist_x) {
            best_dist_x = dr;
            final_x = t.value - width;
            best_line_x = {SnapLine::Vertical, t.value};
            has_x = true;
        }
    }

    if (has_x) {
        m_current_snap_lines.push_back(best_line_x);
    }

    // ---- Y 轴吸附（只对齐上下边缘） ----
    float best_dist_y = m_snap_threshold + 1.0f;
    SnapLine best_line_y;
    bool has_y = false;

    for (const auto &t: horz_targets) {
        float dt = std::abs(top - t.value);
        if (dt < best_dist_y) {
            best_dist_y = dt;
            final_y = t.value;
            best_line_y = {SnapLine::Horizontal, t.value};
            has_y = true;
        }

        float db = std::abs(bottom - t.value);
        if (db < best_dist_y) {
            best_dist_y = db;
            final_y = t.value - height;
            best_line_y = {SnapLine::Horizontal, t.value};
            has_y = true;
        }
    }

    if (has_y) {
        m_current_snap_lines.push_back(best_line_y);
    }

    return QPointF(final_x, final_y);
}

void Scene::snap_to_fullscreen(Source *source) {
    if (!source) return;

    const float canvas_w = 1920.0f;
    const float canvas_h = 1080.0f;
    const float threshold = 25.0f;

    QRectF bounds = source->get_bounding_rect();
    float x = bounds.x();
    float y = bounds.y();
    float w = bounds.width();
    float h = bounds.height();

    bool snap_left = (std::abs(x) <= threshold);
    bool snap_right = (std::abs(x + w - canvas_w) <= threshold);
    bool snap_top = (std::abs(y) <= threshold);
    bool snap_bottom = (std::abs(y + h - canvas_h) <= threshold);
    bool near_full_w = (std::abs(w - canvas_w) <= threshold);
    bool near_full_h = (std::abs(h - canvas_h) <= threshold);

    bool corner_snap = (snap_left || snap_right) && (snap_top || snap_bottom) && near_full_w && near_full_h;
    bool edge_snap_w = snap_left && snap_right && near_full_h;
    bool edge_snap_h = snap_top && snap_bottom && near_full_w;

    if (!corner_snap && !edge_snap_w && !edge_snap_h) return;

    // ✅ 等比缩放满屏
    float scale_x = canvas_w / source->base_width;
    float scale_y = canvas_h / source->base_height;
    float scale = std::max(scale_x, scale_y);

    source->pos_x = (canvas_w - source->base_width * scale) / 2.0f;
    source->pos_y = (canvas_h - source->base_height * scale) / 2.0f;
    source->scale_x = scale;
    source->scale_y = scale;
}

void Scene::collect_snap_targets(Source *exclude_source,
                                 std::vector<SnapTarget> &out_vertical,
                                 std::vector<SnapTarget> &out_horizontal) {
    const float canvas_w = 1920.0f;
    const float canvas_h = 1080.0f;

    // 画布边界（垂直方向用 X，水平方向用 Y）
    out_vertical.push_back({0.0f, SnapTarget::CanvasEdge, nullptr});
    out_vertical.push_back({canvas_w, SnapTarget::CanvasEdge, nullptr});
    out_horizontal.push_back({0.0f, SnapTarget::CanvasEdge, nullptr});
    out_horizontal.push_back({canvas_h, SnapTarget::CanvasEdge, nullptr});

    // 其他源的边
    for (Source *src: sources) {
        if (src == exclude_source || !src->visible) continue;

        QRectF bounds = src->get_bounding_rect();

        // 垂直边 → X 轴吸附
        out_vertical.push_back({static_cast<float>(bounds.left()), SnapTarget::SourceEdge, src});
        out_vertical.push_back({static_cast<float>(bounds.right()), SnapTarget::SourceEdge, src});

        // 水平边 → Y 轴吸附
        out_horizontal.push_back({static_cast<float>(bounds.top()), SnapTarget::SourceEdge, src});
        out_horizontal.push_back({static_cast<float>(bounds.bottom()), SnapTarget::SourceEdge, src});
    }
}

void Scene::render_hover_box() {
    if (!m_hovered_source || !m_hovered_source->visible) return;
    // 如果正在选中这个源，不画悬停框（选中框已覆盖）
    if (m_hovered_source == m_selected_source) return;

    QRectF bounds = m_hovered_source->get_bounding_rect();
    float x = bounds.x();
    float y = bounds.y();
    float w = bounds.width();
    float h = bounds.height();

    // 白色半透明边框，比选中框细
    glLineWidth(2.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.6f);

    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();

    glLineWidth(1.0f);
}
