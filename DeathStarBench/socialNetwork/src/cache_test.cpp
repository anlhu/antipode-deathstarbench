#include <iostream>
#include <thread>
#include <chrono>
#include "cache.h"

// Test: Killing a sleeper thread prevents invalidation
void test_cancel() {
    int prior_count = invalidation_count;
    MessageCache cache;
    Message message = {1, "Test message", "Test carrier"};
    cache.addSentMessage(message);
    cache.cache.collection[1]->killThread();
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME + 1));
    
    if (invalidation_count == prior_count) {
        std::cout << "test_cancel passed\n";
    } else {
        std::cout << "test_cancel failed: expected " << prior_count 
                  << ", got " << invalidation_count << "\n";
    }
}

// Test: Receiving a message removes it from the cache before timeout
void test_receive_message() {
    int prior_count = invalidation_count;
    MessageCache cache;
    cache.addSentMessage({1, "Test message", "Test carrier"});
    cache.receiveMessage({1, "Test message", "Test carrier"});

    bool cacheEmpty = cache.cache.collection.empty();
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME + 1));

    if (cacheEmpty && invalidation_count == prior_count) {
        std::cout << "test_receive_message passed\n";
    } else {
        std::cout << "test_receive_message failed\n";
    }
}

// Test: A single message times out and is removed
void test_1_timeout() {
    int prior_count = invalidation_count;
    MessageCache cache;
    cache.addSentMessage({1, "Test message", "Test carrier"});
    
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME + 1));
    
    if (invalidation_count == prior_count + 1 && cache.cache.collection.empty()) {
        std::cout << "test_1_timeout passed\n";
    } else {
        std::cout << "test_1_timeout failed\n";
    }
}

// Test: Adding two messages, one expires before the other
void test_2_timeout() {
    int prior_count = invalidation_count;
    MessageCache cache;
    cache.addSentMessage({1, "Message 1", "Carrier 1"});
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    cache.addSentMessage({2, "Message 2", "Carrier 2"});
    
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME - 2));
    
    bool firstExpired = invalidation_count == prior_count + 1;
    bool secondStillExists = cache.cache.collection.size() == 1 && cache.cache.collection.count(2);
    
    std::this_thread::sleep_for(std::chrono::duration<double>(LIFETIME));
    
    bool secondExpired = invalidation_count == prior_count + 2;
    bool cacheEmpty = cache.cache.collection.empty();
    
    if (firstExpired && secondStillExists && secondExpired && cacheEmpty) {
        std::cout << "test_2_timeout passed\n";
    } else {
        std::cout << "test_2_timeout failed\n";
    }
}

void test_total_invalidations() {
    if (invalidation_count == 3) {
        std::cout << "test_total_invalidations passed\n";
    } else {
        std::cout << "test_total_invalidations failed\n";
    }
}

int main() {
    std::cout << "Running test 1\n";
    test_cancel();

    std::cout << "Running test 2\n";
    test_receive_message();
    
    std::cout << "Running test 3\n";
    test_1_timeout();
    
    std::cout << "Running test 4\n";
    test_2_timeout();

    std::cout << "Running test 5\n";
    test_total_invalidations();

    return 0;
}
