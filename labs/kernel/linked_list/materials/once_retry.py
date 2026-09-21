# 教学模型：用一把锁演示失败不发布与后续重试，不模拟 CPU 内存序。
from concurrent.futures import ThreadPoolExecutor
from threading import Lock

init_lock = Lock()
state = None
attempts = 0

def get_tasks():
    global state, attempts
    with init_lock:
        if state is None:
            attempts += 1
            candidate = [10]
            if attempts == 1:
                # 私有候选失败，尚未写入共享 state。
                raise MemoryError("模拟构建失败")
            candidate.extend([20, 30])
            state = tuple(candidate)
        return state

try:
    get_tasks()
except MemoryError:
    pass
assert state is None
with ThreadPoolExecutor(max_workers=2) as workers:
    results = list(workers.map(lambda unused: get_tasks(), range(2)))
assert attempts == 2
assert results[0] is results[1]
assert results[0] == (10, 20, 30)
print("尝试次数:", attempts)
print("两个调用者共享完整结果:", results[0])
