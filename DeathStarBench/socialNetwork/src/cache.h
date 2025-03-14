#include <ctime>
#include <functional>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

// Assuming this would be in constants.h
// const double LIFETIME = 60.0; // Example value, should match your actual constant
const double LIFETIME = 3.0; // DEBUGGING VALUE

// Forward declaration
void invalidation_thread(int id);

// Message structure
struct Message {
    int id;
    std::string text;
    std::string carrier;
};

// Represents a single entry in the cache
class CacheEntry {
public:
    int id;
    double timestamp;

    // Extract ID from the message
    int extractId(const Message& message) {
        return message.id;
    }

    explicit CacheEntry(const Message& message) {
        id = extractId(message);
        timestamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
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
public:
    std::unique_ptr<std::thread> wakeupThread;
    int wakeupId;
    std::function<void()> invalidateOldestQueueEntry;
    std::mutex mutex;
    std::condition_variable cv;
    bool isRunning;
    bool cancelRequested;

    // Calculate remaining time before expiration
    double remainingTime(double startTime) {
        double elapsedTime = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() - startTime;
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
        cancelRequested = false;
        
        wakeupThread = std::make_unique<std::thread>([this, entry]() {
            std::unique_lock<std::mutex> lock(mutex);
            double sleepTime = remainingTime(entry.getStartTime());
            if (cv.wait_for(lock, std::chrono::milliseconds(static_cast<int>(sleepTime * 1000)), [this] { return cancelRequested; })) {
                // Cancelled
                std::cout << "THREAD: Timer cancelled for ID: "  << std::endl;
                return;
            }
            
            if (isRunning) {
                std::cout << "THREAD: calling invalidation thread" << std::endl;
                callInvalidationThread();
            }
        });
    }

    // Cancel current timer
    void cancelTimer() {
        std::cout << "Timer cancelled called for ID: " << wakeupId << std::endl;
        std::lock_guard<std::mutex> lock(mutex);
        if (wakeupThread && isRunning) {
            isRunning = false;
            cancelRequested = true;
            cv.notify_all();
            if (wakeupThread->joinable()) {
                wakeupThread->join();
            }
            wakeupThread.reset();
            std::cout << "Timer successfully cancelled for ID: " << wakeupId << std::endl;
        }
    }

    // Call invalidation function
    void callInvalidationThread() {
        invalidation_thread(wakeupId);
        invalidateOldestQueueEntry();
    }

    explicit WakeupThread(std::function<void()> invalidateFunc) 
        : wakeupId(0), invalidateOldestQueueEntry(invalidateFunc), isRunning(false), cancelRequested(false) {}

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
public:
    std::vector<CacheEntry> queue;
    std::unique_ptr<WakeupThread> wakeupThread;
    std::mutex queueMutex;

    // Get the oldest entry in the queue
    const CacheEntry* getOldest() {
        // std::lock_guard<std::mutex> lock(queueMutex);
        std::cout << "Getting oldest entry" << std::endl;
        if (!queue.empty()) {
            return &queue[0];
        }
        return nullptr;
    }

    // Invalidate the oldest entry in the queue
    void invalidateOldestQueueEntry() {
        std::cout << "Invalidating oldest entry" << std::endl;
        std::cout << queue.empty() << std::endl;

        std::lock_guard<std::mutex> lock(queueMutex);
        if (!queue.empty()) {
            std::cout << "starting invalidation" << std::endl;
            queue.erase(queue.begin());
            std::cout << "erased" << std::endl;
            const CacheEntry* nextOldest = getOldest();
            std::cout << "observing invalidation" << std::endl;
            wakeupThread->observeInvalidation(nextOldest);
            std::cout << "observed invalidation" << std::endl;
        }
    }

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
public:
    Queue queue;

    MessageCache() = default;

    void addSentMessage(const Message& message) {
        queue.add(message);
    }

    void receiveMessage(const Message& message) {
        queue.remove(message);
    }
};

int invalidation_count = 0;

// Implementation of the invalidation_thread function (placeholder)
void invalidation_thread(int id) {
    std::cout << "Message with ID: " << id << " has expired." << std::endl;
    invalidation_count++;
    // Additional logic for handling expired messages
}