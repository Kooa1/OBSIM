#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <memory>
#include <iomanip>

#include "core/scene.h"
#include "core/base/source.h"

class DummySource : public Source {
public:
    DummySource(int id, float x, float y, float w, float h)
        : m_id(id) {
        pos_x = x; pos_y = y; base_width = w; base_height = h;
    }
    void render() override {}
    bool update_frame() override { return false; }
    const char* source_type_name() const override { return "Dummy"; }
    int id() const { return m_id; }
private:
    int m_id;
};

struct SceneResult {
    const char* label;
    int sources;
    int iterations;
    double ms;
    double ops_per_sec;
};

static void print_result(const SceneResult& r) {
    std::cout << "  " << std::left << std::setw(45) << r.label
              << "  " << r.iterations << " iters in "
              << std::setw(8) << std::fixed << std::setprecision(1) << r.ms
              << " ms  =>  " << std::setw(10) << std::fixed << std::setprecision(0)
              << r.ops_per_sec << " ops/sec\n";
}

int main() {
    constexpr int N = 1'000'000;
    constexpr int SOURCE_COUNTS[] = {10, 50, 200};
    constexpr float CANVAS_W = 1920.0f;
    constexpr float CANVAS_H = 1080.0f;

    std::cout << "============================================\n";
    std::cout << "  Scene Hit-Test Performance Benchmark\n";
    std::cout << "============================================\n\n";

    std::mt19937 rng(42);

    for (int src_count : SOURCE_COUNTS) {
        std::cout << "--- Sources: " << src_count << " ---\n";

        std::vector<std::unique_ptr<DummySource>> owners;
        Scene scene;

        for (int i = 0; i < src_count; ++i) {
            auto src = std::make_unique<DummySource>(
                i,
                rng() % static_cast<int>(CANVAS_W),
                rng() % static_cast<int>(CANVAS_H),
                50 + rng() % 200,
                50 + rng() % 200);
            scene.add_source(src.get());
            owners.push_back(std::move(src));
        }

        // generate random test points
        std::vector<QPointF> test_points(N);
        for (int i = 0; i < N; ++i) {
            test_points[i] = QPointF(
                static_cast<float>(rng() % static_cast<int>(CANVAS_W)),
                static_cast<float>(rng() % static_cast<int>(CANVAS_H)));
        }

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) {
            scene.hit_test(test_points[i]);
        }
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        print_result({"hit_test (random points)", src_count, N, ms, N / ms * 1000});

        // all hits to first source
        QPointF first_src_pos(owners[0]->pos_x + 10, owners[0]->pos_y + 10);
        start = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) {
            scene.hit_test(first_src_pos);
        }
        end = std::chrono::steady_clock::now();
        ms = std::chrono::duration<double, std::milli>(end - start).count();
        print_result({"hit_test (same point, first source)", src_count, N, ms, N / ms * 1000});

        // add_source / remove_source
        DummySource tmp(-1, 0, 0, 100, 100);
        start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i) {
            scene.add_source(&tmp);
            scene.remove_source(&tmp);
        }
        end = std::chrono::steady_clock::now();
        ms = std::chrono::duration<double, std::milli>(end - start).count();
        print_result({"add/remove_source (100K iterations)", src_count, 100000, ms, 100000 / ms * 1000});

        std::cout << "\n";
    }

    return 0;
}
