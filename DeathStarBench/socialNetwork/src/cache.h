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
const double LIFETIME = 5.0; // DEBUGGING VALUE

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
    void startTimer(const CacheEntry& entry, bool haveMutex = false) {
        std::cout << "WakeupThread: starting new timer thread for " << entry.getId() << std::endl;
        if (!haveMutex) {
            std::lock_guard<std::mutex> lock(mutex);
            std::cout << "WakeupThread: lock acquired for starting new thread" << std::endl;
        } else {
            std::cout << "WakeupThread: already have lock for starting new thread" << std::endl;
            // mutex.unlock();
        }
        
        if (wakeupThread && isRunning) {
            std::cout << "WakeupThread: killing old one" << std::endl;
            cancelTimer();
        }
        
        wakeupId = entry.getId();
        isRunning = true;
        cancelRequested = false;
        
        std::cout << "WakeupThread: actually starting new thread" << std::endl;
        wakeupThread = std::make_unique<std::thread>([this, entry, haveMutex]() {
            // if (!haveMutex) {
            //     std::cout << "WakeupThread: gonna acquire this mf mutex" << std::endl;
            //     std::unique_lock<std::mutex> lock(mutex);
            //     std::cout << "WakeupThread: got that mf mutex" << std::endl;
            // } else {
            //     std::cout << "WakeupThread: already had mutex" << std::endl;
            // }
            std::cout << "WakeupThread: gonna acquire this mf mutex" << std::endl;
            if (haveMutex) {
                mutex.unlock();
            }
            std::unique_lock<std::mutex> lock(mutex);
            std::cout << "WakeupThread: got that mf mutex" << std::endl;
            double sleepTime = remainingTime(entry.getStartTime());
            if (cv.wait_for(lock, std::chrono::milliseconds(static_cast<int>(sleepTime * 1000)), [this] { return cancelRequested; })) {
                // Cancelled
                std::cout << "WakeupThread: THREAD: Timer cancelled for ID: "  << std::endl;
                return;
            }
            
            if (isRunning) {
                std::cout << "WakeupThread: THREAD: calling invalidation thread" << std::endl;
                callInvalidationThread();
            }

            std::cout << "WakeupThread: THREAD: done" << std::endl;
        });

        std::cout << "WakeupThread: done starting timer" << std::endl;
    }

    // Cancel current timer
    void cancelTimer(bool calledFromThread = false) {
        std::cout << "WakeupThread: Timer cancel called for ID: " << wakeupId << std::endl;
        // std::lock_guard<std::mutex> lock(mutex);
        // std::cout << "WakeupThread: timer cancel lock acquired" << std::endl;
        if (wakeupThread && isRunning) {
            std::cout << "WakeupThread: there is a thread running, have to kill" << std::endl;
            isRunning = false;
            cancelRequested = true;
            cv.notify_all();
            if (!calledFromThread && wakeupThread->joinable()) {
                wakeupThread->join();
                wakeupThread.reset();
            }
            std::cout << "WakeupThread: Timer successfully cancelled for ID: " << wakeupId << std::endl;
        }
    }

    // Call invalidation function
    void callInvalidationThread() {
        invalidation_thread(wakeupId);
        std::cout << "WakeupThread: THREAD: invalidation function done" << std::endl;
        invalidateOldestQueueEntry();
        std::cout << "WakeupThread: THREAD: queue handling done" << std::endl;
    }

    explicit WakeupThread(std::function<void()> invalidateFunc) 
        : wakeupId(0), invalidateOldestQueueEntry(invalidateFunc), isRunning(false), cancelRequested(false) {}

    ~WakeupThread() {
        std::cout << "WakeupThread: Destructor called" << std::endl;
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
            std::cout << "WakeupThread: Observing a removed entry" << std::endl;
            cancelTimer();
            std::cout << "WakeupThread: cancelTimer call done, next is: " << next << std::endl;
            if (next != nullptr) {
                startTimer(*next);
            }
        }
    }

    // Observer method for invalidations
    void observeInvalidation(const CacheEntry* nextEntry) {
        std::cout << "WakeupThread: Observing an invalidation" << std::endl;
        cancelTimer(true);
        std::cout << "WakeupThread: timer cancelled" << std::endl;
        if (nextEntry != nullptr) {
            std::cout << "WakeupThread: starting timer after cancel" << std::endl;
            startTimer(*nextEntry, true);
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
        if (!queue.empty()) {
            return &queue[0];
        }
        return nullptr;
    }

    // Invalidate the oldest entry in the queue
    void invalidateOldestQueueEntry() {
        std::cout << "Queue: " << "Invalidating oldest entry" << std::endl;
        std::cout <<"Queue: empty?: " <<  queue.empty() << std::endl;
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!queue.empty()) {
            std::cout << "Queue: " << "starting invalidation" << std::endl;
            queue.erase(queue.begin());
            const CacheEntry* nextOldest = getOldest();
            wakeupThread->observeInvalidation(nextOldest);
            std::cout << "Queue: " << "observed invalidation" << std::endl;
        }
        std::cout << "Queue: " << "done" << std::endl;
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