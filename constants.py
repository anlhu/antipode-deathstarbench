### DEFAULT CONFIG

# Called with threading.Timer
# Make it do something besides print? How??
LIFETIME = 60


def invalidation_thread(name) -> None:
    print(f"{name} failed to receive response in time")


### TEST CONFIG
# LIFETIME = 3

invalidation_count = 0


# def invalidation_thread(name):
#     print(f"{name} failed to receive response in time")
#     global invalidation_count
#     invalidation_count += 1
