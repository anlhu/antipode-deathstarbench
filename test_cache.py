from cache import *
import constants


def test_queue():
    prior_count = constants.invalidation_count
    oldest_time = time.time()
    cache = MessageCache()
    for i in range(10):
        # print("added message", i)
        cache.add_sent_message(i)
    cache.queue.wakeup_thread._cancel_timer()
    while cache.queue.queue:
        popped = cache.queue.queue.pop(0)
        # print("popped", popped.get_start_time())
        assert popped.get_start_time() > oldest_time
        oldest_time = popped.get_start_time()
    assert constants.invalidation_count == prior_count == 0


def test_cancel():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    cache.queue.wakeup_thread._cancel_timer()
    time.sleep(LIFETIME)
    assert constants.invalidation_count == prior_count == 0


def test_invalidating_thread():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    time.sleep(LIFETIME)
    # print("waiting done")
    # print(constants.invalidation_count)
    assert (
        constants.invalidation_count == prior_count + 1
    ), f"constants.invalidation_count: {constants.invalidation_count}, prior_count: {prior_count}"
    assert cache.queue.queue == []


def test_2_invalidating_thread():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    time.sleep(2)
    cache.add_sent_message(2)
    time.sleep(LIFETIME - 2)
    # print("waiting done")
    # print(constants.invalidation_count)
    assert (
        constants.invalidation_count == prior_count + 1
    ), f"constants.invalidation_count: {constants.invalidation_count}, prior_count: {prior_count}"
    assert len(cache.queue.queue) == 1 and cache.queue.queue[0].get_id() == 2
    time.sleep(LIFETIME)
    assert (
        constants.invalidation_count == prior_count + 2
    ), f"constants.invalidation_count: {constants.invalidation_count}, prior_count: {prior_count}"


if __name__ == "__main__":
    test_queue()
    print("test 1 done")

    test_cancel()
    print("test 2 done")

    test_invalidating_thread()
    print("test 3 done")

    test_2_invalidating_thread()
    print("test 4 done")
