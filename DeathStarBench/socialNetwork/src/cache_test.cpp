#include <iostream>
#include <thread>
#include <cassert>
#include "cache.h"

void testQueue() {
    int priorCount = invalidation_count;
    
    // Use std::chrono::steady_clock::now() for timestamps
    double oldestTime = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    // std::cout << "Oldest time: " << oldestTime << std::endl;

    MessageCache cache;
    for (int i = 0; i < 10; ++i) {
        Message message;
        message.id = i;
        cache.addSentMessage(message);
    }
    
    cache.queue.wakeupThread->cancelTimer();

    while (!cache.queue.queue.empty()) {
        auto popped = cache.queue.queue.front();
        cache.queue.queue.erase(cache.queue.queue.begin());

        // Compare time_points correctly
        // std::cout << "Popped time: " << popped.getStartTime() << std::endl;
        // std::cout << "Oldest time: " << oldestTime << std::endl;
        assert(popped.getStartTime() > oldestTime);
        oldestTime = popped.getStartTime();
    }

    assert(invalidation_count == priorCount);
}

void testCancel() {
    int priorCount = invalidation_count;
    MessageCache cache;
    Message message;
    message.id = 1;
    cache.addSentMessage(message);
    cache.queue.wakeupThread->cancelTimer();

    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME));

    assert(invalidation_count == priorCount);
}

void testInvalidatingThread() {
    int priorCount = invalidation_count;
    MessageCache cache;
    Message message;
    message.id = 1;
    cache.addSentMessage(message);

    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME + 1));

    std::cout << "Invalidation count: " << invalidation_count << std::endl;
    std::cout << "Prior count: " << priorCount << std::endl;
    assert(invalidation_count == priorCount + 1);
    assert(cache.queue.queue.empty());
    assert(cache.queue.wakeupThread->isRunning == false);
    std::cout << "test 3 done" << std::endl;
}

void testTwoInvalidatingThread() {
    int priorCount = invalidation_count;
    MessageCache cache;
    Message message1;
    message1.id = 1;
    cache.addSentMessage(message1);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    Message message2;
    message2.id = 2;
    cache.addSentMessage(message2);
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME - 2));

    assert(invalidation_count == priorCount + 1);
    assert(cache.queue.queue.size() == 1 && cache.queue.queue.front().getId() == 2);

    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME));

    assert(invalidation_count == priorCount + 2);
}

int main() {
    // testQueue();
    // std::cout << "test 1 done" << std::endl;

    // testCancel();
    // std::cout << "test 2 done" << std::endl;

    std::cout << "test 3 starting" << std::endl;
    testInvalidatingThread();
    std::cout << "test 3 done" << std::endl;

    testTwoInvalidatingThread();
    std::cout << "test 4 done" << std::endl;

    return 0;
}
