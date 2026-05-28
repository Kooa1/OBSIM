#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <optional>
#include "utils/datasafequeue.h"

class DataSafeQueueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DataSafeQueueTest, SingleThreadedPushPopFifo) {
    DataSafeQueue<int> queue(128);

    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(queue.push(i));
    }

    for (int i = 0; i < 100; ++i) {
        auto item = queue.pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, i);
    }
}

TEST_F(DataSafeQueueTest, TryPopOnEmptyReturnsNullopt) {
    DataSafeQueue<int> queue(128);

    auto item = queue.try_pop();
    EXPECT_FALSE(item.has_value());
}

TEST_F(DataSafeQueueTest, TryPushToFullQueueReturnsFalse) {
    DataSafeQueue<int> queue(5);

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue.try_push(i));
    }

    bool result = queue.try_push(99);
    EXPECT_FALSE(result);
}

TEST_F(DataSafeQueueTest, PopWithDrainKeepsLastItem) {
    DataSafeQueue<int> queue(128);

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(queue.push(i));
    }

    int out = -1;
    bool result = queue.pop_with_drain(out);

    EXPECT_TRUE(result);
    EXPECT_EQ(out, 9);
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, PopWithDrainOnEmptyReturnsFalse) {
    DataSafeQueue<int> queue(128);

    int out = -1;
    bool result = queue.pop_with_drain(out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out, -1);
}

TEST_F(DataSafeQueueTest, TryPopDrainKeepsNewest) {
    DataSafeQueue<int> queue(128);

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(queue.push(i));
    }

    auto result = queue.try_pop_drain();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 9);
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, TryPopDrainOnEmptyReturnsNullopt) {
    DataSafeQueue<int> queue(128);

    auto result = queue.try_pop_drain();
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataSafeQueueTest, PushNoWaitDiscardsOldestWhenFull) {
    DataSafeQueue<int> queue(5);

    for (int i = 0; i < 5; ++i) {
        queue.push_no_wait(i);
    }

    queue.push_no_wait(99);

    EXPECT_EQ(queue.get_queue_size(), 5);

    auto first = queue.pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 1);
}

TEST_F(DataSafeQueueTest, PushPoisonPill) {
    DataSafeQueue<int> queue(128);

    queue.push_poison_pill();

    EXPECT_FALSE(queue.is_empty());

    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 0);
}

TEST_F(DataSafeQueueTest, CleanQueueEmptiesAllItems) {
    DataSafeQueue<int> queue(128);

    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(queue.push(i));
    }

    EXPECT_FALSE(queue.is_empty());
    EXPECT_EQ(queue.get_queue_size(), 50);

    queue.clean_queue();

    EXPECT_TRUE(queue.is_empty());
    EXPECT_EQ(queue.get_queue_size(), 0);
}

TEST_F(DataSafeQueueTest, CleanQueueResetsStopState) {
    DataSafeQueue<int> queue(128);

    queue.stop_work();
    queue.clean_queue();

    ASSERT_TRUE(queue.push(42));
    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 42);
}

TEST_F(DataSafeQueueTest, IsEmptyReportsCorrectly) {
    DataSafeQueue<int> queue(128);

    EXPECT_TRUE(queue.is_empty());

    ASSERT_TRUE(queue.push(1));
    EXPECT_FALSE(queue.is_empty());

    queue.pop();
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, GetQueueSizeReportsCorrectly) {
    DataSafeQueue<int> queue(128);

    EXPECT_EQ(queue.get_queue_size(), 0);

    for (int i = 0; i < 25; ++i) {
        ASSERT_TRUE(queue.push(i));
    }
    EXPECT_EQ(queue.get_queue_size(), 25);

    for (int i = 0; i < 10; ++i) {
        queue.pop();
    }
    EXPECT_EQ(queue.get_queue_size(), 15);

    queue.clean_queue();
    EXPECT_EQ(queue.get_queue_size(), 0);
}

TEST_F(DataSafeQueueTest, StopWorkPreventsPop) {
    DataSafeQueue<int> queue(128);

    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));

    queue.stop_work();

    auto item = queue.pop();
    EXPECT_FALSE(item.has_value());
}

TEST_F(DataSafeQueueTest, StopWorkWithTryPopReturnsNullopt) {
    DataSafeQueue<int> queue(128);

    ASSERT_TRUE(queue.push(1));
    queue.stop_work();

    auto item = queue.try_pop();
    EXPECT_FALSE(item.has_value());
}

TEST_F(DataSafeQueueTest, MultipleProducersSingleConsumer) {
    DataSafeQueue<int> queue(256);
    std::atomic<int> produced_count(0);
    std::atomic<int> consumed_count(0);
    constexpr int items_per_producer = 200;
    constexpr int num_producers = 4;
    constexpr int total_items = items_per_producer * num_producers;

    std::vector<int> consumed_values;
    std::mutex consumed_mutex;

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, &produced_count, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int value = p * items_per_producer + i;
                while (!queue.push(value)) {
                    std::this_thread::yield();
                }
                produced_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::thread consumer([&]() {
        int consumed = 0;
        while (consumed < total_items) {
            auto item = queue.pop();
            if (item.has_value()) {
                {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_values.push_back(*item);
                }
                consumed_count.fetch_add(1, std::memory_order_relaxed);
                ++consumed;
            }
        }
    });

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    EXPECT_EQ(produced_count.load(), total_items);
    EXPECT_EQ(consumed_count.load(), total_items);
    EXPECT_EQ(consumed_values.size(), total_items);

    std::sort(consumed_values.begin(), consumed_values.end());
    for (int i = 0; i < total_items; ++i) {
        EXPECT_EQ(consumed_values[i], i);
    }
}

TEST_F(DataSafeQueueTest, MultipleProducersTryPushContention) {
    DataSafeQueue<int> queue(32);
    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);
    constexpr int num_producers = 4;
    constexpr int attempts_per_producer = 100;

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, &success_count, &fail_count, attempts_per_producer]() {
            for (int i = 0; i < attempts_per_producer; ++i) {
                if (queue.try_push(i)) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    fail_count.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    EXPECT_EQ(success_count.load() + fail_count.load(),
              num_producers * attempts_per_producer);
    EXPECT_LE(queue.get_queue_size(), 32);
}

TEST_F(DataSafeQueueTest, PushAfterCleanQueueSucceeds) {
    DataSafeQueue<int> queue(128);

    queue.stop_work();

    auto pop_result = queue.pop();
    EXPECT_FALSE(pop_result.has_value());

    queue.clean_queue();

    ASSERT_TRUE(queue.push(42));
    auto result = queue.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(DataSafeQueueTest, FifoOrderPreservedUnderConcurrentAccess) {
    DataSafeQueue<int> queue(1024);
    constexpr int num_items = 500;

    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread consumer([&]() {
        for (int i = 0; i < num_items; ++i) {
            auto item = queue.pop();
            ASSERT_TRUE(item.has_value());
            EXPECT_EQ(*item, i);
        }
    });

    producer.join();
    consumer.join();
}

TEST_F(DataSafeQueueTest, TryPopOnQueueWithItemsReturnsValues) {
    DataSafeQueue<int> queue(128);

    ASSERT_TRUE(queue.push(10));
    ASSERT_TRUE(queue.push(20));
    ASSERT_TRUE(queue.push(30));

    auto item = queue.try_pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 10);

    item = queue.try_pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 20);

    item = queue.try_pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 30);

    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, PushAndPopMultipleItems) {
    DataSafeQueue<int> queue(16);

    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(queue.push(i));
    }
    EXPECT_EQ(queue.get_queue_size(), 10);

    for (int i = 0; i < 10; i++) {
        auto item = queue.pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, i);
    }
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, TryPopDrainWithSingleItem) {
    DataSafeQueue<int> queue(128);

    ASSERT_TRUE(queue.push(42));

    auto result = queue.try_pop_drain();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(DataSafeQueueTest, PopWithDrainSingleItem) {
    DataSafeQueue<int> queue(128);

    ASSERT_TRUE(queue.push(99));

    int out = -1;
    bool result = queue.pop_with_drain(out);

    EXPECT_TRUE(result);
    EXPECT_EQ(out, 99);
    EXPECT_TRUE(queue.is_empty());
}