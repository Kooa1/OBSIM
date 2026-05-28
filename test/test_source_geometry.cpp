#include <gtest/gtest.h>

#include <QApplication>
#include <QPointF>
#include <QRectF>

#include "core/base/source.h"

class TestSource : public Source {
public:
    TestSource(int w, int h, qreal x, qreal y)
        : Source()
    {
        base_width = static_cast<float>(w);
        base_height = static_cast<float>(h);
        pos_x = static_cast<float>(x);
        pos_y = static_cast<float>(y);
    }

    void render() override {}
    const char* source_type_name() const override { return "TestSource"; }

    using Source::canvas_to_local;

    QPointF local_to_canvas(const QPointF& local_pos) const {
        float canvas_x = (local_pos.x() - crop_left) * scale_x + pos_x;
        float canvas_y = (local_pos.y() - crop_top) * scale_y + pos_y;
        return QPointF(canvas_x, canvas_y);
    }
};

class SourceGeometryTest : public ::testing::Test {
protected:
    void SetUp() override {
        source = std::make_unique<TestSource>(1920, 1080, 100.0, 200.0);
    }

    std::unique_ptr<TestSource> source;
};

TEST_F(SourceGeometryTest, BoundingRect) {
    QRectF bounds = source->get_bounding_rect();
    EXPECT_FLOAT_EQ(100.0f, static_cast<float>(bounds.x()));
    EXPECT_FLOAT_EQ(200.0f, static_cast<float>(bounds.y()));
    EXPECT_FLOAT_EQ(1920.0f, static_cast<float>(bounds.width()));
    EXPECT_FLOAT_EQ(1080.0f, static_cast<float>(bounds.height()));
}

TEST_F(SourceGeometryTest, HitTestInside) {
    QPointF inside(source->pos_x + 10.0, source->pos_y + 10.0);
    EXPECT_TRUE(source->hit_test(inside));
}

TEST_F(SourceGeometryTest, HitTestOutside) {
    QPointF outside(source->pos_x - 100.0, source->pos_y - 100.0);
    EXPECT_FALSE(source->hit_test(outside));
}

TEST_F(SourceGeometryTest, ContentRect) {
    QRectF content = source->get_content_rect();
    EXPECT_FLOAT_EQ(source->crop_left, static_cast<float>(content.x()));
    EXPECT_FLOAT_EQ(source->crop_top, static_cast<float>(content.y()));
    float expected_w = source->base_width - source->crop_left - source->crop_right;
    float expected_h = source->base_height - source->crop_top - source->crop_bottom;
    EXPECT_FLOAT_EQ(expected_w, static_cast<float>(content.width()));
    EXPECT_FLOAT_EQ(expected_h, static_cast<float>(content.height()));
}

TEST_F(SourceGeometryTest, CanvasToLocalRoundTrip) {
    QPointF canvas_pt(source->pos_x + 200.0, source->pos_y + 300.0);
    QPointF local_pt = source->canvas_to_local(canvas_pt);
    QPointF back_to_canvas = source->local_to_canvas(local_pt);
    EXPECT_FLOAT_EQ(static_cast<float>(canvas_pt.x()), static_cast<float>(back_to_canvas.x()));
    EXPECT_FLOAT_EQ(static_cast<float>(canvas_pt.y()), static_cast<float>(back_to_canvas.y()));
}

TEST_F(SourceGeometryTest, BoundingRectWithScale) {
    source->scale_x = 2.0f;
    source->scale_y = 0.5f;
    QRectF bounds = source->get_bounding_rect();
    EXPECT_FLOAT_EQ(100.0f, static_cast<float>(bounds.x()));
    EXPECT_FLOAT_EQ(200.0f, static_cast<float>(bounds.y()));
    EXPECT_FLOAT_EQ(1920.0f * 2.0f, static_cast<float>(bounds.width()));
    EXPECT_FLOAT_EQ(1080.0f * 0.5f, static_cast<float>(bounds.height()));
}

TEST_F(SourceGeometryTest, HitTestInvisible) {
    source->visible = false;
    QPointF inside(source->pos_x + 10.0, source->pos_y + 10.0);
    EXPECT_FALSE(source->hit_test(inside));
}

TEST_F(SourceGeometryTest, CanvasToLocalWithCrop) {
    source->crop_left = 50.0f;
    source->crop_top = 30.0f;
    QPointF canvas_pt(source->pos_x + 100.0, source->pos_y + 200.0);
    QPointF local_pt = source->canvas_to_local(canvas_pt);
    float expected_local_x = (100.0f) / source->scale_x + source->crop_left;
    float expected_local_y = (200.0f) / source->scale_y + source->crop_top;
    EXPECT_FLOAT_EQ(expected_local_x, static_cast<float>(local_pt.x()));
    EXPECT_FLOAT_EQ(expected_local_y, static_cast<float>(local_pt.y()));
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}