#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <random>
#include <cstring>

#include "core/recoder.h"
#include "core/base/outputchannel.h"

// ---------------------------------------------------------------------------
// NullOutputChannel -- counts packets, signals consumer
// ---------------------------------------------------------------------------
class CountingOutput : public OutputChannel {
public:
    std::atomic<int> packets{0};
    std::mutex mtx;
    std::condition_variable cv;
    int target = 0;

    bool start(const std::string&, AVCodecParameters*, AVCodecParameters*,
               AVRational, AVRational) override { return true; }
    void stop() override {}
    void feed_packet(AVPacketPtr) override {
        packets.fetch_add(1);
        cv.notify_one();
    }
    bool is_running() const override { return true; }

    void wait_until(int n) {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&]{ return packets.load() >= n; });
    }
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_frame(int w, int h) {
    std::vector<uint8_t> frame(w * h * 4);
    static std::mt19937 rng(42);
    for (auto& b : frame) b = static_cast<uint8_t>(rng() & 0xFF);
    return frame;
}

struct RecoderResult {
    const char* label;
    int    width;
    int    height;
    int    frames;
    double ms_total;
    double ms_per_frame;
    double fps;
};

static void print_result(const RecoderResult& r) {
    std::cout << "  " << std::left << std::setw(50) << r.label
              << std::right << "  " << r.frames << " frames in "
              << std::setw(8) << std::fixed << std::setprecision(1) << r.ms_total
              << " ms  |  " << std::setw(7) << std::fixed << std::setprecision(2)
              << r.ms_per_frame << " ms/frame  |  "
              << std::setw(5) << std::fixed << std::setprecision(1) << r.fps << " fps\n";
}

static RecoderResult bench_recoder(int w, int h, int fps, int frames, const char* label) {
    std::vector<uint8_t> frame_data = make_frame(w, h);
    int stride = w * 4;

    Recoder recoder;
    CountingOutput output;
    recoder.register_output(&output);

    recoder.start("", w, h, fps);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // warm-up

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        recoder.feed_frame(frame_data.data(), stride, w, h);
    }
    recoder.stop();
    auto end = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    int encoded = output.packets.load();

    return {label, w, h, encoded, ms, ms / std::max(1, encoded), encoded / (ms / 1000.0)};
}

int main() {
    std::cout << "============================================\n";
    std::cout << "  Recoder (x264) Encoding Benchmark\n";
    std::cout << "============================================\n";
    std::cout << "  Codec    : libx264 (preset=veryfast, tune=zerolatency)\n";
    std::cout << "  Bitrate  : 10 Mbps\n\n";

    // test different resolutions, each with 300 frames at 30 fps target
    struct ResTest { int w, h, fps, frames; const char* label; };
    ResTest tests[] = {
        {1920, 1080, 30, 300, "1080p @ 30 fps   (300 frames)"},
        {1280,  720, 30, 300, "720p  @ 30 fps   (300 frames)"},
        { 854,  480, 30, 300, "480p  @ 30 fps   (300 frames)"},
        {1920, 1080, 60, 300, "1080p @ 60 fps   (300 frames)"},
    };

    for (auto& t : tests) {
        auto r = bench_recoder(t.w, t.h, t.fps, t.frames, t.label);
        print_result(r);
    }

    // per-frame latency distribution for 1080p
    std::cout << "\n--- Per-frame latency (1080p, 100 frames) ---\n";
    {
        int w = 1920, h = 1080, frames = 100;
        auto frame_data = make_frame(w, h);
        int stride = w * 4;

        Recoder recoder;
        CountingOutput output;
        recoder.register_output(&output);

        recoder.start("", w, h, 30);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<double> latencies;
        latencies.reserve(frames);

        for (int i = 0; i < frames; ++i) {
            auto t0 = std::chrono::steady_clock::now();
            recoder.feed_frame(frame_data.data(), stride, w, h);
            output.wait_until(static_cast<int>(latencies.size()) + 1);
            auto t1 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        recoder.stop();

        double sum = 0, min_v = 1e9, max_v = 0;
        for (auto v : latencies) {
            sum += v;
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
        double avg = sum / latencies.size();

        std::sort(latencies.begin(), latencies.end());
        double p50 = latencies[latencies.size() * 50 / 100];
        double p95 = latencies[latencies.size() * 95 / 100];
        double p99 = latencies[latencies.size() * 99 / 100];

        std::cout << "  Min  : " << std::fixed << std::setprecision(3) << min_v << " ms\n";
        std::cout << "  Avg  : " << std::fixed << std::setprecision(3) << avg  << " ms\n";
        std::cout << "  P50  : " << std::fixed << std::setprecision(3) << p50  << " ms\n";
        std::cout << "  P95  : " << std::fixed << std::setprecision(3) << p95  << " ms\n";
        std::cout << "  P99  : " << std::fixed << std::setprecision(3) << p99  << " ms\n";
        std::cout << "  Max  : " << std::fixed << std::setprecision(3) << max_v << " ms\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
