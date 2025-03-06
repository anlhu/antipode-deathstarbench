from heapq import *
import time
from threading import Timer

# The idea is to have a single Timer thread that will print when the oldest message has expired
# Using a Timer thread since they can be cancelled, apparently threads can't be killed


# Called with threading.Timer
# Make it do something besides print? How??
def invalidation_thread(name) -> None:
    print(f"{name} failed to receive response in time")


# Manages the sleeping thread
# Can observe the stream of entries that are added to or removed from the heap
#   If heap tells you oldest entry, can decide when to kill the sleeper and replace it with next entry
class WakeupThread:
    LIFETIME_SECONDS = 60

    def __init__(self) -> None:
        self.wakeup_thread: None | Timer = None
        self.wakeup_id: None | int = None

    def _start_timer(self, entry: "CacheEntry") -> None:
        self.wakeup_id = entry.get_id()
        self.wakeup_thread = Timer(
            self._remaining_time(entry.get_start_time()),
            invalidation_thread,
            args=[self.wakeup_id],
        )
        self.wakeup_thread.start()

    def _cancel_timer(self) -> None:
        if self.wakeup_thread is not None:
            self.wakeup_thread.cancel()

    def _remaining_time(self, start_time: float) -> float:
        elapsed_time = time.time() - start_time
        return self.LIFETIME_SECONDS - elapsed_time

    def observe_new_entry(self, entry: "CacheEntry") -> None:
        if self.wakeup_thread is None:
            self._start_timer(entry)

    def observe_remove_entry(self, entry: "CacheEntry", next: "CacheEntry") -> None:
        if entry.get_id() == self.wakeup_id:
            self._cancel_timer()
            self._start_timer(next)


# The heap is a min-heap, so the oldest message is at the top
# CacheEntry implements __lt__ so the heap can compare objects
# CacheEntry implements __eq__ so we can remove objects from the heap
class Heap:

    def __init__(self) -> None:
        self.heap: list["CacheEntry"] = []
        heapify(self.heap)
        self.wakeup_thread = WakeupThread()

    def _get_oldest(self) -> "CacheEntry":
        return self.heap[0]

    def add(self, message) -> None:
        entryObj = CacheEntry(message)
        heappush(self.heap, entryObj)
        self.wakeup_thread.observe_new_entry(entryObj)

    def remove(self, message) -> None:
        searchObj = CacheEntry(message)
        removedEntry = self.heap.pop(self.heap.index(searchObj))
        heapify(self.heap)
        self.wakeup_thread.observe_remove_entry(removedEntry, self._get_oldest())


# Represents a single entry in the cache
# We only want the message's ID and the time it was sent
# If it's a message received, we will just ignore it's timestamp - not great but good enough for now
# Python's built in heap uses __lt__ and __eq__ to compare objects, so do it for the heap
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

    def __lt__(self, other: "CacheEntry") -> bool:
        return self.timestamp < other.timestamp

    def __eq__(self, other: "CacheEntry") -> bool:
        return self.id == other.id


# This is just an interface for the message queue to use
class MessageCache:

    def __init__(self) -> None:
        self.heap = Heap()

    def add_sent_message(self, message) -> None:
        self.heap.add(message)

    def receive_message(self, message):
        self.heap.remove(message)
