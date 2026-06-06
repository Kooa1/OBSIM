#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iomanip>
#include "utils/datasafequeue.h"

static void print_result(const char* name, long long ops, double ms) {
    std::cout << "  " << std::left << std::setw(45) << name
              << std::right << std::setw(10) << ops
              << " ops in " << std::setw(8) << std::fixed << std::setprecision(1) << ms
              << " ms  =>  " << std::setw(10) << std::fixed << std::setprecision(0)
              << (ops / ms * 1000) << " ops/sec\n";
}

int main() {
    constexpr int N = 1'000'000;
    constexpr int BIG = N + 100;   // large enough to never block in single-thread tests
    constexpr int P = 4;

    std::cout << "============================================\n";
    std::cout << "  DataSafeQueue Performance Benchmark\n";
    std::cout << "============================================\n";
    std::cout << "  Iterations : " << N << "\n\n";

    // 1. single-thread push (unbounded: queue never fills)
    {
        DataSafeQueue<int> q(BIG);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) q.push(i);
        auto t1 = std::chrono::steady_clock::now();
        print_result("push (single, never-full)", N,
                     std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    // 2. single-thread pop
    {
        DataSafeQueue<int> q(BIG);
        for (int i = 0; i < N; ++i) q.push(i);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) q.pop();
        auto t1 = std::chrono::steady_clock::now();
        print_result("pop (single)", N,
                     std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    // 3. single-thread push_no_wait (capacity 256, discards when full)
    {
        DataSafeQueue<int> q(256);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) q.push_no_wait(i);
        auto t1 = std::chrono::steady_clock::now();
        print_result("push_no_wait (single, cap=256, lossy)", N,
                     std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    // 4. 1P1C ping-pong (capacity 1, tight coupling)
    {
        DataSafeQueue<int> q(1);
        constexpr int PN = 10000;
        auto t0 = std::chrono::steady_clock::now();

        std::thread producer([&]() {
            for (int i = 0; i < PN; ++i) q.push(i);
        });

        for (int i = 0; i < PN; ++i) q.pop();
        producer.join();
        print_result("push/pop (1P1C, cap=1)", PN,
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }

    // 5. 1P1C with cap=256
    {
        DataSafeQueue<int> q(256);
        constexpr int PN = 100000;
        auto t0 = std::chrono::steady_clock::now();

        std::thread producer([&]() {
            for (int i = 0; i < PN; ++i) q.push(i);
        });

        for (int i = 0; i < PN; ++i) q.pop();
        producer.join();
        print_result("push/pop (1P1C, cap=256)", PN,
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }

    // 6. push_no_wait 4P (cap=256, lossy)
    {
        DataSafeQueue<int> q(256);
        std::atomic<long long> pushed{0};
        constexpr int PN = 100000;

        auto t0 = std::chrono::steady_clock::now();

        std::vector<std::thread> producers;
        for (int p = 0; p < P; ++p) {
            producers.emplace_back([&]() {
                int per = PN / P;
                for (int i = 0; i < per; ++i) {
                    q.push_no_wait(i);
                    pushed.fetch_add(1);
                }
            });
        }
        for (auto& t : producers) t.join();

        print_result("push_no_wait (4P, cap=256, lossy)", pushed.load(),
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }

    return 0;
}
