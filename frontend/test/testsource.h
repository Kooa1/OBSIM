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
        // 🔧 必须在这里设置默认基准尺寸
        base_width = 400;
        base_height = 300;
        visible = true;
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
