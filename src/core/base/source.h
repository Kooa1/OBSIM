#ifndef OBSIM_SOURCE_H
#define OBSIM_SOURCE_H

#include <QString>
#include <QRectF>
#include <QPointF>
#include <QMatrix4x4>
#include <memory>
#include <string>

/// @brief Base class for all source types (image, video, etc.) in the scene.
/// Provides geometry properties (position, scale, rotation, crop) and
/// declares rendering interfaces for subclass implementation.
class Source {
public:
    Source();
    virtual ~Source();

    /// @brief Renders the source. OpenGL ortho and clip region are set before calling.
    virtual void render() = 0;

    /// @brief Updates per-frame data (e.g. video texture). Returns true if content changed.
    virtual bool update_frame() { return false; }

    /// @brief Returns the display name of the source type (e.g. "Image", "Video").
    virtual const char* source_type_name() const = 0;

    /// @brief Returns the bounding rectangle in canvas space, accounting for position, scale and rotation.
    /// Default returns an axis-aligned box without rotation.
    virtual QRectF get_bounding_rect() const;

    /// @brief Returns the content rectangle after cropping (in local coordinates).
    virtual QRectF get_content_rect() const;

    /// @brief Hit test: determines whether a canvas-space point falls inside the source.
    /// @param canvas_pos Canvas logical coordinate (1920x1080 space).
    virtual bool hit_test(const QPointF& canvas_pos) const;

    /// @brief Loads resources (e.g. textures for video/image sources).
    virtual void load_resources() {}
    /// @brief Unloads resources.
    virtual void unload_resources() {}

    /// @brief Returns the preview texture ID for thumbnail display.
    virtual uint32_t get_preview_texture_id() const { return 0; }

protected:
    /// @brief Computes the transformation matrix from local coordinates to canvas coordinates.
    QMatrix4x4 get_local_to_canvas_transform() const;

    /// @brief Returns the final render width (base width * scale after cropping).
    float get_render_width() const;
    /// @brief Returns the final render height (base height * scale after cropping).
    float get_render_height() const;

    /// @brief Converts a canvas coordinate to local source coordinate (used in hit_test).
    QPointF canvas_to_local(const QPointF& canvas_pos) const;

private:
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;

public:
    std::string id;                     ///< Unique identifier (for serialization and lookup).
    QString display_name;               ///< UI display name.

    float pos_x = 0.0f;                 ///< Source X position in canvas space (top-left corner).
    float pos_y = 0.0f;                 ///< Source Y position in canvas space (top-left corner).

    float base_width = 0.0f;            ///< Base width of the source (original size without scaling).
    float base_height = 0.0f;           ///< Base height of the source (original size without scaling).

    float scale_x = 1.0f;               ///< Horizontal scale factor (1.0 = original size).
    float scale_y = 1.0f;               ///< Vertical scale factor (1.0 = original size).

    float rotation = 0.0f;              ///< Rotation angle in degrees (around the source center).

    float crop_left = 0.0f;             ///< Left crop amount in source-local pixels.
    float crop_top = 0.0f;              ///< Top crop amount in source-local pixels.
    float crop_right = 0.0f;            ///< Right crop amount in source-local pixels.
    float crop_bottom = 0.0f;           ///< Bottom crop amount in source-local pixels.
    float fixed_aspect_ratio = 16.0f / 9.0f;  ///< Fixed aspect ratio.

    bool visible = true;                ///< Visibility flag.
    bool selected = false;              ///< Selection flag (set by the scene manager).
    bool lock_aspect_ratio = false;     ///< Whether to lock aspect ratio during scaling.
};

using SourcePtr = std::unique_ptr<Source>;

#endif //OBSIM_SOURCE_H