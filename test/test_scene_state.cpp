#include <gtest/gtest.h>

#include <QApplication>
#include <QPointF>
#include <QRectF>

#include "core/base/source.h"
#include "core/scene.h"

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
};

class SceneStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        scene = std::make_unique<Scene>();
        source_a = std::make_unique<TestSource>(200, 200, 50.0, 50.0);
        source_b = std::make_unique<TestSource>(200, 200, 300.0, 300.0);
        source_c = std::make_unique<TestSource>(200, 200, 600.0, 600.0);
    }

    QPointF center_of(const TestSource* src) const {
        return QPointF(src->pos_x + src->base_width * src->scale_x / 2.0f,
                       src->pos_y + src->base_height * src->scale_y / 2.0f);
    }

    QPointF point_inside(const TestSource* src) const {
        return QPointF(src->pos_x + src->base_width / 2.0f,
                       src->pos_y + src->base_height / 2.0f);
    }

    std::unique_ptr<Scene> scene;
    std::unique_ptr<TestSource> source_a;
    std::unique_ptr<TestSource> source_b;
    std::unique_ptr<TestSource> source_c;
};

TEST_F(SceneStateTest, AddSource) {
    scene->add_source(source_a.get());
    ASSERT_EQ(1, scene->get_sources().size());
    EXPECT_EQ(source_a.get(), scene->get_sources()[0]);
}

TEST_F(SceneStateTest, AddSourceDuplicate) {
    scene->add_source(source_a.get());
    scene->add_source(source_a.get());
    ASSERT_EQ(1, scene->get_sources().size());
}

TEST_F(SceneStateTest, RemoveSource) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    ASSERT_EQ(2, scene->get_sources().size());

    scene->remove_source(source_a.get());
    ASSERT_EQ(1, scene->get_sources().size());
    EXPECT_EQ(source_b.get(), scene->get_sources()[0]);
}

TEST_F(SceneStateTest, RemoveSourceNotPresent) {
    scene->add_source(source_a.get());
    scene->remove_source(source_b.get());
    ASSERT_EQ(1, scene->get_sources().size());
}

TEST_F(SceneStateTest, RemoveSourceClearsSelection) {
    scene->add_source(source_a.get());
    scene->set_selected_source(source_a.get());
    EXPECT_EQ(source_a.get(), scene->selected_source());

    scene->remove_source(source_a.get());
    EXPECT_EQ(nullptr, scene->selected_source());
}

TEST_F(SceneStateTest, MultipleSources) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    scene->add_source(source_c.get());
    ASSERT_EQ(3, scene->get_sources().size());
}

TEST_F(SceneStateTest, HitTestOnSource) {
    scene->add_source(source_a.get());
    QPointF inside = point_inside(source_a.get());
    EXPECT_EQ(source_a.get(), scene->hit_test(inside));
}

TEST_F(SceneStateTest, HitTestMiss) {
    scene->add_source(source_a.get());
    QPointF outside(source_a->pos_x - 100.0, source_a->pos_y - 100.0);
    EXPECT_EQ(nullptr, scene->hit_test(outside));
}

TEST_F(SceneStateTest, HitTestReturnsTopMost) {
    source_b->pos_x = 50.0;
    source_b->pos_y = 50.0;

    scene->add_source(source_a.get());
    scene->add_source(source_b.get());

    QPointF overlap(150.0, 150.0);
    EXPECT_EQ(source_b.get(), scene->hit_test(overlap));
}

TEST_F(SceneStateTest, HitTestInvisibleSource) {
    source_a->visible = false;
    scene->add_source(source_a.get());

    QPointF inside = point_inside(source_a.get());
    EXPECT_EQ(nullptr, scene->hit_test(inside));
}

TEST_F(SceneStateTest, MoveToTop) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    scene->add_source(source_c.get());

    scene->move_to_top(source_a.get());
    EXPECT_EQ(source_a.get(), scene->get_sources().back());
}

TEST_F(SceneStateTest, MoveToBottom) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    scene->add_source(source_c.get());

    scene->move_to_bottom(source_c.get());
    EXPECT_EQ(source_c.get(), scene->get_sources().front());
}

TEST_F(SceneStateTest, MoveUp) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    scene->add_source(source_c.get());

    scene->move_up(source_b.get());
    EXPECT_EQ(source_c.get(), scene->get_sources()[1]);
    EXPECT_EQ(source_b.get(), scene->get_sources()[2]);
}

TEST_F(SceneStateTest, MoveDown) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());
    scene->add_source(source_c.get());

    scene->move_down(source_b.get());
    EXPECT_EQ(source_b.get(), scene->get_sources()[0]);
    EXPECT_EQ(source_a.get(), scene->get_sources()[1]);
}

TEST_F(SceneStateTest, SelectedSourceInitiallyNull) {
    EXPECT_EQ(nullptr, scene->selected_source());
}

TEST_F(SceneStateTest, SetSelectedSource) {
    scene->add_source(source_a.get());
    scene->set_selected_source(source_a.get());
    EXPECT_EQ(source_a.get(), scene->selected_source());
    EXPECT_TRUE(source_a->selected);
}

TEST_F(SceneStateTest, SetSelectedSourceClearsPrevious) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());

    scene->set_selected_source(source_a.get());
    scene->set_selected_source(source_b.get());

    EXPECT_EQ(source_b.get(), scene->selected_source());
    EXPECT_FALSE(source_a->selected);
    EXPECT_TRUE(source_b->selected);
}

TEST_F(SceneStateTest, ClearSelection) {
    scene->add_source(source_a.get());
    scene->set_selected_source(source_a.get());
    scene->clear_selection();
    EXPECT_EQ(nullptr, scene->selected_source());
    EXPECT_FALSE(source_a->selected);
}

TEST_F(SceneStateTest, MousePressSelectsSource) {
    scene->add_source(source_a.get());

    QPointF inside = point_inside(source_a.get());
    bool handled = scene->on_mouse_press(inside);

    EXPECT_TRUE(handled);
    EXPECT_EQ(source_a.get(), scene->selected_source());
}

TEST_F(SceneStateTest, MousePressOnEmptyClearsSelection) {
    scene->add_source(source_a.get());
    scene->set_selected_source(source_a.get());

    QPointF empty(0.0, 0.0);
    bool handled = scene->on_mouse_press(empty);

    EXPECT_FALSE(handled);
    EXPECT_EQ(nullptr, scene->selected_source());
    EXPECT_EQ(InteractionMode::None, scene->interaction_mode());
}

TEST_F(SceneStateTest, MouseMoveDragsSource) {
    scene->add_source(source_a.get());

    QPointF press_pos = point_inside(source_a.get());
    scene->on_mouse_press(press_pos);

    QPointF move_pos(press_pos.x() + 50.0, press_pos.y() + 30.0);
    bool needs_redraw = scene->on_mouse_move(move_pos);

    EXPECT_TRUE(needs_redraw);
}

TEST_F(SceneStateTest, MouseReleaseResetsMode) {
    scene->add_source(source_a.get());

    QPointF inside = point_inside(source_a.get());
    scene->on_mouse_press(inside);

    scene->on_mouse_release();
    EXPECT_EQ(InteractionMode::None, scene->interaction_mode());
}

TEST_F(SceneStateTest, MouseMoveWithoutPressDoesNotDrag) {
    scene->add_source(source_a.get());

    QPointF move_pos = point_inside(source_a.get());
    scene->on_mouse_move(move_pos);

    EXPECT_EQ(InteractionMode::None, scene->interaction_mode());
}

TEST_F(SceneStateTest, MousePositionUpdatedOnMove) {
    scene->add_source(source_a.get());
    QPointF pos(123.0, 456.0);
    scene->on_mouse_move(pos);
    EXPECT_FLOAT_EQ(123.0f, scene->mouse_canvas_pos().x());
    EXPECT_FLOAT_EQ(456.0f, scene->mouse_canvas_pos().y());
}

TEST_F(SceneStateTest, MoveToTopAlreadyTop) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());

    scene->move_to_top(source_b.get());
    ASSERT_EQ(2, scene->get_sources().size());
    EXPECT_EQ(source_b.get(), scene->get_sources().back());
}

TEST_F(SceneStateTest, MoveToBottomAlreadyBottom) {
    scene->add_source(source_a.get());
    scene->add_source(source_b.get());

    scene->move_to_bottom(source_a.get());
    ASSERT_EQ(2, scene->get_sources().size());
    EXPECT_EQ(source_a.get(), scene->get_sources().front());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}