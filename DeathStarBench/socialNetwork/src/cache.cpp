#include "cache.h"

int invalidation_count = 0;

void invalidation_thread(int id) {
	std::cout << "Message with ID: " << id << " has expired." << std::endl;
    invalidation_count++;
}

// WakeupThread Implementation
WakeupThread::WakeupThread(double sleepTime, CacheEntry* entry)
    : sleepTime(sleepTime), entry(entry), isCancelled(false) {
    thread = std::make_unique<std::thread>(&WakeupThread::threadFunction, this);
}

WakeupThread::~WakeupThread() {
    cancelTimer();
}

void WakeupThread::cancelTimer() {
    isCancelled = true;
    if (thread && thread->joinable()) {
        thread->detach();
    }
}

void WakeupThread::threadFunction() {
    std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
    if (!isCancelled) {
        invalidation_thread(entry->getId());
        entry->callCacheTimeoutHandler();
    }
}

// CacheEntry Implementation
CacheEntry::CacheEntry(int id, std::string message, std::function<void()> cacheHandler)
    : id(id), message(std::move(message)), cacheTimeoutHandler(std::move(cacheHandler)) {
    thread = std::make_unique<WakeupThread>(LIFETIME, this);
}

int CacheEntry::getId() const {
    return id;
}

void CacheEntry::callCacheTimeoutHandler() {
    cacheTimeoutHandler();
}

void CacheEntry::killThread() {
    thread->cancelTimer();
}

// Cache Implementation
void Cache::add(Message message) {
    int id = extractId(message);
    collection[id] = std::make_unique<CacheEntry>(id, message.text, [this, id]() { this->removeById(id); });
}

void Cache::remove(Message message) {
    int id = extractId(message);
    if (collection.count(id)) {
        collection[id]->killThread();
        removeById(id);
    }
}

int Cache::extractId(const Message& message) {
    return message.id;
}

void Cache::removeById(int id) {
    collection.erase(id);
}

// MessageCache Implementation
void MessageCache::addSentMessage(Message message) {
    cache.add(message);
}

void MessageCache::receiveMessage(Message message) {
    cache.remove(message);
}
