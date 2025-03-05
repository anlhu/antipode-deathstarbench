from heapq import *
import time



class CacheEntry:

	def __init__(self, id: int) -> None:
		self.id = id
		self.timestamp = time.time()

	def __lt__(self, other: "CacheEntry") -> bool:
		return self.timestamp < other.timestamp


class MessageCache:
	LIFETIME_SECONDS = 60

	def __init__(self) -> None:
		self.heap = []
		heapify(self.heap)
		# will have one thread that just sleeps until the oldest entry hits wakeup time
		# if its responded to, kill the thread and make a new wakeup thread for the next oldest entry
		self.wakeup_thread = None

	def add_sent_message(self, message: str) -> None:
		msg_obj = CacheEntry(MessageCache.hash(message))
		heappush(self.heap, msg_obj)

	def receive_message(self, message: str) -> bool:
		msg_id = MessageCache.hash(message)
		# remove corresponding msg from heap while keeping it a heap
		# 	have to replace it, then sink it?
		# 	or maybe just remove it and then call heapify? less efficient but easier

		# update next invalidation time
		
		# spawn a thread/kill them for wakeups? if its the oldest?
		pass

	def get_oldest_message(self) -> CacheEntry:
		return self.heap[0]

	def get_invalidation_time(self, entry: CacheEntry) -> float:
		return entry.timestamp + self.LIFETIME_SECONDS

	def get_next_invalidation_time(self) -> float:
		return self.get_invalidation_time(self.heap[0])

	def hash(self, message: str) -> int:
		return hash(message)