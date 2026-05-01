//
// Created by 66 on 2026/4/29.
//

#ifndef OBSIM_SOURCE_H
#define OBSIM_SOURCE_H

#include <QString>
#include <QRectF>
#include <QPointF>
#include <QMatrix4x4>
#include <memory>
#include <string>

class Source {
public:
    Source();
    virtual ~Source();

    // ========== 纯虚函数：子类必须实现 ==========
    // 渲染该源
    // 在调用此函数前，glOrtho 和裁剪区域已设置好
    virtual void render() = 0;

    // 返回源的显示名称（如 "Image", "Video" 等）
    virtual const char* source_type_name() const = 0;

    // ========== 虚函数：子类可选择重写 ==========

    // 返回源在画布空间中的边界矩形（考虑位置、缩放、旋转）
    // 默认返回未旋转的轴对齐包围盒
    virtual QRectF get_bounding_rect() const;

    // 获取源的内容矩形（考虑裁剪后的实际内容区域，本地坐标）
    virtual QRectF get_content_rect() const;

    // 命中测试：判断画布坐标点是否在源内
    // canvas_pos 为画布逻辑坐标（1920x1080空间）
    virtual bool hit_test(const QPointF& canvas_pos) const;

    // 资源加载/卸载（用于视频、图片等需要管理的纹理资源）
    virtual void load_resources() {}
    virtual void unload_resources() {}

    // 获取缩略图等预览用
    virtual uint32_t get_preview_texture_id() const { return 0; }

protected:
    // ========== 辅助函数供子类使用 ==========

    // 获取从本地坐标到画布坐标的变换矩阵
    QMatrix4x4 get_local_to_canvas_transform() const;

    // 获取最终渲染尺寸（基准尺寸 × 缩放）
    float get_render_width() const;
    float get_render_height() const;

    // 将画布坐标转换为本地坐标（用于 hit_test）
    QPointF canvas_to_local(const QPointF& canvas_pos) const;

private:
    // 禁用拷贝
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;

    public:
    // 唯一标识符（用于序列化、查找等）
    std::string id;

    // ========== 几何属性（画布逻辑坐标， 1920x1080） ==========
    // 源在画布上的位置（左上角）
    float pos_x = 0.0f;
    float pos_y = 0.0f;

    // 源的基准尺寸（原始素材尺寸，无缩放时）
    float base_width = 0.0f;
    float base_height = 0.0f;

    // 缩放比例（1.0 = 原始大小）
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    // 旋转角度（度数，围绕源的中心点旋转）
    float rotation = 0.0f;

    // 裁剪（相对于源自身的坐标，像素单位）
    // 0 表示从对应边不裁剪，正值表示裁剪掉该边的像素数
    float crop_left = 0.0f;
    float crop_top = 0.0f;
    float crop_right = 0.0f;
    float crop_bottom = 0.0f;

    // 可见性
    bool visible = true;

    // 是否被选中（由场景管理类设置）
    bool selected = false;

    // 锁定宽高比（缩放时是否保持原始比例）
    bool lock_aspect_ratio = false;
};

// 智能指针类型别名
using SourcePtr = std::unique_ptr<Source>;


#endif //OBSIM_SOURCE_H
