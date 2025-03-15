#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <unordered_map>
#include <memory>
#include <thread>
#include <functional>
#include <chrono>

extern int invalidation_count; 
void invalidation_thread(int id);

const double LIFETIME = 5.0; // Debugging value

// Message structure
struct Message {
    int id;
    std::string text;
    std::string carrier;
};

// Forward declare CacheEntry to avoid circular dependencies
class CacheEntry;

// Manages the sleeper thread
class WakeupThread {
public:
    WakeupThread(double sleepTime, CacheEntry* entry);
    ~WakeupThread();
    void cancelTimer();

    void threadFunction();

    double sleepTime;
    CacheEntry* entry;
    std::unique_ptr<std::thread> thread;
    bool isCancelled;
};

// Represents a single entry in the cache
class CacheEntry {
public:
    CacheEntry(int id, std::string message, std::function<void()> cacheHandler);
    int getId() const;
    void callCacheTimeoutHandler();
    void killThread();

    int id;
    std::string message;
    std::function<void()> cacheTimeoutHandler;
    std::unique_ptr<WakeupThread> thread;
};

// Cache class
class Cache {
public:
    void add(struct Message message);
    void remove(struct Message message);

    std::unordered_map<int, std::unique_ptr<CacheEntry>> collection;
    int extractId(const struct Message& message);
    void removeById(int id);
};

// Interface for message queue
class MessageCache {
public:
    void addSentMessage(struct Message message);
    void receiveMessage(struct Message message);

    Cache cache;
};

#endif // CACHE_H
