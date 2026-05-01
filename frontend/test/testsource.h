#ifndef OBSIM_RECTSOURCE_H
#define OBSIM_RECTSOURCE_H

#include <GL/gl.h>

#include "core/source.h"

class TestSource : public Source {
public:
    float color_r = 0.0f;
    float color_g = 0.0f;
    float color_b = 0.0f;
    float color_a = 1.0f;

    TestSource() {
        // 默认 16:9 比例，320×180 作为基准
        base_width = 1920.0f;
        base_height = 1080.0f;
        visible = true;
        lock_aspect_ratio = true;       // 锁定比例
        fixed_aspect_ratio = 16.0f / 9.0f;
    }

    void render() override {
        // 启用颜色
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // 🔧 关键：必须用base_width和base_height来计算顶点
        // 因为投影矩阵会应用scale_x/scale_y
        glColor4f(color_r, color_g, color_b, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(base_width, 0.0f);
        glVertex2f(base_width, base_height);
        glVertex2f(0.0f, base_height);
        glEnd();

        glDisable(GL_BLEND);
    }

    const char* source_type_name() const override { return "Test"; }
};

#endif //OBSOBSIM_RECTSOURCE_H
