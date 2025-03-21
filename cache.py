import time
from threading import Timer
from constants import *

# The idea is to have a single Timer thread that will print when the oldest message has expired
#   Using a Timer thread since they can be cancelled, apparently threads can't be killed
# Use a queue for cache so you know the oldest message
# TODO: Need to add unique id to messages from the DeathStarBench message queue, and integrate with MessageCache
# TODO: Need to update the CacheEntry _extract_id method to work with JSON


# Manages the sleeper thread
# Start the sleeper on init
# Can cancel the sleeper
class WakeupThread:

    def __init__(self, sleepTime, entry) -> None:
        self.sleepTime = sleepTime
        self.entry = entry
        self.thread = self._start_timer()

    def _start_timer(self) -> Timer:
        wakeup_thread = Timer(
            self.sleepTime,
            self._thread_function_wrapper,
            args=[],
        )
        wakeup_thread.start()
        return wakeup_thread

    def _thread_function_wrapper(self):
        invalidation_thread(self.entry.get_id())
        self.entry.call_cache_timeout_handler()

    def cancel_timer(self) -> None:
        self.thread.cancel()


# Represents a single entry in the cache
# We only want the message's ID and the time it was sent
class CacheEntry:

    def __init__(self, id, message, cache_handler) -> None:
        self.id = id
        self.message = message
        self.cache_timeout_handler = cache_handler
        self.thread = WakeupThread(LIFETIME, self)

    def get_id(self) -> int:
        return self.id

    def call_cache_timeout_handler(self):
        return self.cache_timeout_handler()

    def kill_thread(self) -> None:
        self.thread.cancel_timer()


# Actual cache that holds the entries
class Cache:

    def __init__(self) -> None:
        self.collection: dict[int, CacheEntry] = {}

    # TODO: probably extracrt message['id'] once its JSON
    def _extract_id(self, message) -> int:
        return hash(message)

    def _delete(self, id) -> None:
        del self.collection[id]

    def add(self, message) -> None:
        id = self._extract_id(message)
        self.collection[id] = CacheEntry(id, message, lambda *_: self._delete(id))

    def remove(self, message) -> None:
        id = self._extract_id(message)
        self.collection[id].kill_thread()
        self._delete(id)


# This is just an interface for the message queue to use
class MessageCache:

    def __init__(self) -> None:
        self.cache = Cache()

    def add_sent_message(self, message) -> None:
        self.cache.add(message)

    def receive_message(self, message):
        self.cache.remove(message)
