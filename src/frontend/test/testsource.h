#ifndef OBSIM_RECTSOURCE_H
#define OBSIM_RECTSOURCE_H

#include <GL/gl.h>

#include "../../core/base/source.h"

/// @brief Test source that renders a colored rectangle for debugging
class TestSource : public Source {
public:
    float color_r = 0.0f; ///< Red component
    float color_g = 0.0f; ///< Green component
    float color_b = 0.0f; ///< Blue component
    float color_a = 1.0f; ///< Alpha component

    TestSource() {
        base_width = 1920.0f;
        base_height = 1080.0f;
        visible = true;
        lock_aspect_ratio = true;
        fixed_aspect_ratio = 16.0f / 9.0f;
    }

    /// @brief Render the test rectangle
    void render() override {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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