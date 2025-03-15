from cache import *
import constants


# test killing a sleeper thread means it doesnt update the invalidation count
def test_cancel():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    cache.cache.collection[1].kill_thread()
    time.sleep(LIFETIME + 1)
    assert constants.invalidation_count == prior_count == 0


def test_receive_message():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    cache.receive_message(1)
    assert cache.cache.collection == {}
    time.sleep(LIFETIME + 1)
    assert constants.invalidation_count == prior_count == 0


def test_1_timeout():
    prior_count = constants.invalidation_count
    cache = MessageCache()
    cache.add_sent_message(1)
    time.sleep(LIFETIME)
    # print("waiting done")
    # print(constants.invalidation_count)
    assert (
        constants.invalidation_count == prior_count + 1
    ), f"constants.invalidation_count: {constants.invalidation_count}, prior_count: {prior_count}"
    assert cache.cache.collection == {}, cache.cache.collection


def test_2_timeout():
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
    assert len(cache.cache.collection) == 1 and cache.cache.collection[2]
    time.sleep(LIFETIME)
    assert (
        constants.invalidation_count == prior_count + 2
    ), f"constants.invalidation_count: {constants.invalidation_count}, prior_count: {prior_count}"
    assert cache.cache.collection == {}, cache.cache.collection


if __name__ == "__main__":
    test_cancel()
    print("test 1 done")

    test_receive_message()
    print("test 2 done")

    test_1_timeout()
    print("test 3 done")

    test_2_timeout()
    print("test 4 done")
