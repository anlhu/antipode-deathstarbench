import time
from threading import Timer
from constants import *

# The idea is to have a single Timer thread that will print when the oldest message has expired
#   Using a Timer thread since they can be cancelled, apparently threads can't be killed
# Use a queue for cache so you know the oldest message
# TODO: Need to add unique id to messages from the DeathStarBench message queue, and integrate with MessageCache
# TODO: Need to update the CacheEntry _extract_id method to work with JSON


# Represents a single entry in the cache
# We only want the message's ID and the time it was sent
# If it's a message received, we will just ignore it's timestamp - not great but good enough for now
class CacheEntry:

    def __init__(self, message) -> None:
        self.id = self._extract_id(message)
        self.timestamp = time.time()

    # TODO: probably extracrt message['id'] once its JSON
    def _extract_id(self, message) -> int:
        return hash(message)

    def get_id(self) -> int:
        return self.id

    def get_start_time(self) -> float:
        return self.timestamp


# Manages the sleeping thread
# Can observe the stream of entries that are added to or removed from the queue
#   If queue tells you oldest entry, can decide when to kill the sleeper and replace it with next entry
# Also oberserves invalidations so it can reset the thread
class WakeupThread:

    def __init__(self, invalidate_oldest_queue_entry) -> None:
        # self.wakeup_thread: None | Timer = None
        # self.wakeup_id: None | int = None
        self.wakeup_thread = None
        self.wakeup_id = None
        self.invalidate_oldest_queue_entry = invalidate_oldest_queue_entry

    def _start_timer(self, entry: CacheEntry) -> None:
        self.wakeup_id = entry.get_id()
        self.wakeup_thread = Timer(
            self._remaining_time(entry.get_start_time()),
            self._call_invalidation_thread,
            args=[],
        )
        self.wakeup_thread.start()

    def _cancel_timer(self) -> None:
        if self.wakeup_thread is not None:
            self.wakeup_thread.cancel()

    def _remaining_time(self, start_time: float) -> float:
        elapsed_time = time.time() - start_time
        remaining_time = LIFETIME - elapsed_time
        return remaining_time

    def _call_invalidation_thread(self) -> None:
        invalidation_thread(self.wakeup_id)
        self.invalidate_oldest_queue_entry()

    def observe_new_entry(self, entry: CacheEntry) -> None:
        if self.wakeup_thread is None:
            self._start_timer(entry)

    # def observe_remove_entry(self, entry: CacheEntry, next: CacheEntry | None) -> None:
    def observe_remove_entry(self, entry: CacheEntry, next) -> None:
        if entry.get_id() == self.wakeup_id:
            self._cancel_timer()
            if next is not None:
                self._start_timer(next)
            else:
                self.wakeup_thread = None
                self.wakeup_id = None

    # def observe_invalidation(self, next_entry: CacheEntry | None) -> None:
    def observe_invalidation(self, next_entry) -> None:
        if next_entry is not None:
            self._start_timer(next_entry)  # handles reset of private variables
        else:
            self.wakeup_thread = None
            self.wakeup_id = None


# The queue is ordered by time
# Also has a method to invalidate the oldest message, but called by invalidation function
class Queue:

    def __init__(self) -> None:
        self.queue: list[CacheEntry] = []
        self.wakeup_thread = WakeupThread(self._invalidate_oldest_queue_entry)

    def _invalidate_oldest_queue_entry(self) -> None:
        self.queue.pop(0)
        next_oldest = self._get_oldest()
        self.wakeup_thread.observe_invalidation(next_oldest)

    # def _get_oldest(self) -> CacheEntry | None:
    def _get_oldest(self):
        if self.queue:
            return self.queue[0]
        return None

    def add(self, message) -> None:
        entryObj = CacheEntry(message)
        self.queue.append(entryObj)
        self.wakeup_thread.observe_new_entry(entryObj)

    def remove(self, message) -> None:
        searchObj = CacheEntry(message)
        removedEntry = self.queue.pop(self.queue.index(searchObj))  # Can binary search
        self.wakeup_thread.observe_remove_entry(removedEntry, self._get_oldest())


# This is just an interface for the message queue to use
class MessageCache:

    def __init__(self) -> None:
        self.queue = Queue()

    def add_sent_message(self, message) -> None:
        self.queue.add(message)

    def receive_message(self, message):
        self.queue.remove(message)
