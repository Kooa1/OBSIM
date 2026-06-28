#ifndef OBSIM_SCENE_H
#define OBSIM_SCENE_H

#include <Windows.h>

#include <vector>
#include <algorithm>
#include <cmath>

#include <GL/gl.h>

#include <QPointF>
#include <QRectF>

#include "base/source.h"

/// @brief Interaction modes for the scene state machine.
enum class InteractionMode {
    None,
    Hover,
    Dragging,
    Resizing,
    Selecting
};

/// @brief Resize handle positions on a selected source.
enum class ResizeHandle {
    None,
    TopLeft, Top, TopRight,
    Left, Right,
    BottomLeft, Bottom, BottomRight
};

/// @brief Manages scene sources, selection, hit-testing, interactive dragging/resizing, and snap alignment.
class Scene {
public:
    static constexpr float CANVAS_W = 1920.0f;
    static constexpr float CANVAS_H = 1080.0f;

    explicit Scene();

    /// @brief Returns all sources for rendering traversal (front-to-back = bottom-to-top).
    std::vector<Source *> &get_sources() { return sources; }
    const std::vector<Source *> &get_sources() const { return sources; }

    /// @brief Adds a source to the scene.
    void add_source(Source *source);

    /// @brief Removes a source from the scene.
    void remove_source(Source *source);

    /// @brief Moves a source to the top (front-most) layer.
    void move_to_top(Source *source);

    /// @brief Moves a source to the bottom (back-most) layer.
    void move_to_bottom(Source *source);

    /// @brief Moves a source one layer up.
    void move_up(Source *source);

    /// @brief Moves a source one layer down.
    void move_down(Source *source);

    /// @brief Returns the currently selected source (single selection).
    Source *selected_source() const { return m_selected_source; }

    /// @brief Sets the currently selected source.
    void set_selected_source(Source *source);

    /// @brief Clears the current selection.
    void clear_selection();

    /// @brief Performs hit-testing from topmost to bottommost source.
    Source *hit_test(const QPointF &canvas_pos) const;

    /// @brief Handles mouse press events for the interaction state machine.
    bool on_mouse_press(const QPointF &canvas_pos);

    /// @brief Handles mouse move events for the interaction state machine.
    bool on_mouse_move(const QPointF &canvas_pos);

    /// @brief Handles mouse release events.
    void on_mouse_release();

    /// @brief Returns the current interaction mode.
    InteractionMode interaction_mode() const { return m_mode; }

    /// @brief Returns the current mouse position in canvas coordinates.
    QPointF mouse_canvas_pos() const { return m_mouse_pos; }

    /// @brief Renders the selection box and resize handles for the selected source.
    void render_selection_box();

    /// @brief Sets the snap threshold in logical canvas pixels.
    void set_snap_threshold(float threshold) { m_snap_threshold = threshold; }
    float snap_threshold() const { return m_snap_threshold; }

    /// @brief Enables or disables snap alignment.
    void set_snap_enabled(bool enabled) { m_snap_enabled = enabled; }
    bool is_snap_enabled() const { return m_snap_enabled; }

    /// @brief Represents a snap guide line for rendering.
    struct SnapLine {
        enum Orientation { Horizontal, Vertical };

        Orientation orientation;
        float position;
    };

    /// @brief Renders snap guide lines.
    void render_snap_lines();

    /// @brief Returns the cursor shape appropriate for the current mouse position.
    Qt::CursorShape get_cursor_for_current_pos() const;

private:
    struct SnapTarget {
        float value;

        enum Type { CanvasEdge, SourceEdge, SourceCenter };

        Type type;
        Source *source;
    };

    ResizeHandle get_handle_at_pos(Source *source, const QPointF &canvas_pos) const;

    Qt::CursorShape cursor_for_handle(ResizeHandle handle) const;

    QPointF snap_position(const QPointF &proposed_pos,
                          float width, float height,
                          Source *exclude_source = nullptr);

    void snap_to_fullscreen(Source* source);

    std::vector<SnapLine> current_snap_lines() const { return m_current_snap_lines; }

    void collect_snap_targets(Source *exclude_source,
                              std::vector<SnapTarget> &out_vertical,
                              std::vector<SnapTarget> &out_horizontal);

    void render_hover_box();

    void update_cursor();

private:
    std::vector<Source *> sources; ///< Source list (raw pointers, lifecycle managed externally by unique_ptr)

    Source *m_selected_source = nullptr; ///< Currently selected source

    InteractionMode m_mode = InteractionMode::None; ///< Current interaction mode

    QPointF m_mouse_pos; ///< Current mouse position on canvas

    QPointF m_drag_start_mouse; ///< Mouse canvas position at drag start
    float m_drag_start_pos_x = 0; ///< Source pos_x at drag start
    float m_drag_start_pos_y = 0; ///< Source pos_y at drag start
    float m_drag_start_width = 0; ///< Source base_width at drag start (for resize)
    float m_drag_start_height = 0; ///< Source base_height at drag start (for resize)
    ResizeHandle m_active_handle = ResizeHandle::None; ///< Currently active resize handle

    float m_snap_threshold = 10.0f; ///< Snap threshold in logical canvas pixels (default 10)
    bool m_snap_enabled = true; ///< Whether snap alignment is enabled

    std::vector<SnapLine> m_current_snap_lines; ///< Snap lines for the current frame (used for rendering)

    float m_drag_start_scale_x = 1.0f; ///< Source scale_x at drag start
    float m_drag_start_scale_y = 1.0f; ///< Source scale_y at drag start

    Source* m_hovered_source = nullptr; ///< Currently hovered source
};

#endif //OBSIM_SCENE_H
