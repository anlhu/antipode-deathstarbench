#include <ctime>
#include <functional>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>

// Assuming this would be in constants.h
const double LIFETIME = 60.0; // Example value, should match your actual constant

// Forward declaration
void invalidation_thread(int id);

// Message structure
struct Message {
    int id;
    std::string timestamp;
};

// Represents a single entry in the cache
class CacheEntry {
private:
    int id;
    double timestamp;

    // Extract ID from the message
    int extractId(const Message& message) {
        return message.id;
    }

public:
    explicit CacheEntry(const Message& message) {
        id = extractId(message);
        timestamp = std::time(nullptr);
    }

    int getId() const {
        return id;
    }

    double getStartTime() const {
        return timestamp;
    }

    bool operator==(const CacheEntry& other) const {
        return id == other.id;
    }
};

// Class to manage timer threads for cache invalidation
class WakeupThread {
private:
    std::unique_ptr<std::thread> wakeupThread;
    int wakeupId;
    std::function<void()> invalidateOldestQueueEntry;
    std::mutex mutex;
    bool isRunning;

    // Calculate remaining time before expiration
    double remainingTime(double startTime) {
        double elapsedTime = std::time(nullptr) - startTime;
        return LIFETIME - elapsedTime;
    }

    // Start timer for an entry
    void startTimer(const CacheEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (wakeupThread && isRunning) {
            cancelTimer();
        }
        
        wakeupId = entry.getId();
        isRunning = true;
        
        wakeupThread = std::make_unique<std::thread>([this, entry]() {
            double sleepTime = remainingTime(entry.getStartTime());
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleepTime * 1000)));
            
            if (isRunning) {
                callInvalidationThread();
            }
        });
    }

    // Cancel current timer
    void cancelTimer() {
        std::lock_guard<std::mutex> lock(mutex);
        if (wakeupThread && isRunning) {
            isRunning = false;
            if (wakeupThread->joinable()) {
                wakeupThread->join();
            }
            wakeupThread.reset();
        }
    }

    // Call invalidation function
    void callInvalidationThread() {
        invalidation_thread(wakeupId);
        invalidateOldestQueueEntry();
    }

public:
    explicit WakeupThread(std::function<void()> invalidateFunc) 
        : wakeupId(0), invalidateOldestQueueEntry(invalidateFunc), isRunning(false) {}

    ~WakeupThread() {
        cancelTimer();
    }

    // Observer method for new entries
    void observeNewEntry(const CacheEntry& entry) {
        if (!wakeupThread || !isRunning) {
            startTimer(entry);
        }
    }

    // Observer method for removed entries
    void observeRemoveEntry(const CacheEntry& entry, const CacheEntry* next) {
        if (entry.getId() == wakeupId) {
            cancelTimer();
            if (next != nullptr) {
                startTimer(*next);
            }
        }
    }

    // Observer method for invalidations
    void observeInvalidation(const CacheEntry* nextEntry) {
        cancelTimer();
        if (nextEntry != nullptr) {
            startTimer(*nextEntry);
        }
    }
};

// Queue class to manage cache entries
class Queue {
private:
    std::vector<CacheEntry> queue;
    std::unique_ptr<WakeupThread> wakeupThread;
    std::mutex queueMutex;

    // Get the oldest entry in the queue
    const CacheEntry* getOldest() {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!queue.empty()) {
            return &queue[0];
        }
        return nullptr;
    }

    // Invalidate the oldest entry in the queue
    void invalidateOldestQueueEntry() {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!queue.empty()) {
            queue.erase(queue.begin());
            const CacheEntry* nextOldest = getOldest();
            wakeupThread->observeInvalidation(nextOldest);
        }
    }

public:
    Queue() {
        wakeupThread = std::make_unique<WakeupThread>(
            std::bind(&Queue::invalidateOldestQueueEntry, this)
        );
    }

    // Add a message to the queue
    void add(const Message& message) {
        std::lock_guard<std::mutex> lock(queueMutex);
        CacheEntry entryObj(message);
        queue.push_back(entryObj);
        wakeupThread->observeNewEntry(entryObj);
    }

    // Remove a message from the queue
    void remove(const Message& message) {
        std::lock_guard<std::mutex> lock(queueMutex);
        CacheEntry searchObj(message);
        
        auto it = std::find_if(queue.begin(), queue.end(), 
            [&searchObj](const CacheEntry& entry) {
                return entry.getId() == searchObj.getId();
            });
            
        if (it != queue.end()) {
            CacheEntry removedEntry = *it;
            queue.erase(it);
            const CacheEntry* oldestEntry = getOldest();
            wakeupThread->observeRemoveEntry(removedEntry, oldestEntry);
        }
    }
};

// Interface class for the message queue
class MessageCache {
private:
    Queue queue;

public:
    MessageCache() = default;

    void addSentMessage(const Message& message) {
        queue.add(message);
    }

    void receiveMessage(const Message& message) {
        queue.remove(message);
    }
};

// Implementation of the invalidation_thread function (placeholder)
void invalidation_thread(int id) {
    std::cout << "Message with ID: " << id << " has expired." << std::endl;
    // Additional logic for handling expired messages
}