"""普通中断的离散教学模型：只演示通知状态与完成义务，不访问硬件。"""
from collections import deque


def new_source(trigger):
    # 锁存边沿与当前电平分开；active 和优先级占用也是两个状态。
    return {"trigger": trigger, "data": deque(), "edge_pending": False,
            "active": False, "priority_busy": False}


def pending(source):
    if source["trigger"] == "level":
        return bool(source["data"])
    return source["edge_pending"]


def receive(source, value):
    # 教学 UART 以 FIFO 非空维持电平；边沿模型每个字节产生一次边沿。
    source["data"].append(value)
    if source["trigger"] == "edge":
        source["edge_pending"] = True


def acknowledge(source):
    # 模型只保留一个来源；其他递送条件假定满足。
    if source["active"] or not pending(source):
        return False
    source["edge_pending"] = False
    source["active"] = True
    source["priority_busy"] = True
    return True


def drain(source):
    result = "".join(source["data"])
    source["data"].clear()
    return result


def eoi(source, split):
    if not source["priority_busy"]:
        raise ValueError("没有尚待优先级下降的确认")
    source["priority_busy"] = False
    if not split:
        source["active"] = False


def deactivate(source):
    if not source["active"] or source["priority_busy"]:
        raise ValueError("本模型要求先确认、再下降优先级、最后停用")
    source["active"] = False


def show(label, source):
    print(f'{label}: pending={int(pending(source))}, '
          f'active={int(source["active"])}, '
          f'priority_busy={int(source["priority_busy"])}, '
          f'data={"".join(source["data"]) or "-"}')


def main():
    # 状态推进独立执行；断言只检查结果，优化模式也保留完整流程。
    normal = new_source("level")
    receive(normal, "A")
    was_acknowledged = acknowledge(normal)
    assert was_acknowledged
    drained_data = drain(normal)
    assert drained_data == "A"
    eoi(normal, split=False)
    show("正常", normal)

    uncleared = new_source("level")
    receive(uncleared, "A")
    was_acknowledged = acknowledge(uncleared)
    assert was_acknowledged
    eoi(uncleared, split=False)  # 故意漏掉设备清源。
    show("漏清源", uncleared)
    was_acknowledged = acknowledge(uncleared)  # 条件仍在，可以再次认领。
    assert was_acknowledged
    drain(uncleared)
    eoi(uncleared, split=False)

    unfinished = new_source("level")
    receive(unfinished, "A")
    was_acknowledged = acknowledge(unfinished)
    assert was_acknowledged
    drain(unfinished)
    eoi(unfinished, split=True)  # 故意暂不执行停用。
    receive(unfinished, "B")
    show("漏停用", unfinished)
    was_acknowledged = acknowledge(unfinished)
    assert not was_acknowledged
    deactivate(unfinished)  # 补齐协议后，才可处理 B。
    was_acknowledged = acknowledge(unfinished)
    assert was_acknowledged
    drain(unfinished)
    eoi(unfinished, split=False)

    merged = new_source("edge")
    receive(merged, "A")
    receive(merged, "B")
    show("确认前两次边沿", merged)
    was_acknowledged = acknowledge(merged)
    assert was_acknowledged
    drained_data = drain(merged)
    assert drained_data == "AB"
    eoi(merged, split=False)

    late = new_source("edge")
    receive(late, "A")
    was_acknowledged = acknowledge(late)
    assert was_acknowledged
    receive(late, "B")  # 已确认以后再次来边沿，锁存新的通知。
    drained_data = drain(late)
    assert drained_data == "AB"
    eoi(late, split=False)
    show("本轮已读走后来数据", late)
    was_acknowledged = acknowledge(late)  # 后一轮可能没有设备数据。
    assert was_acknowledged
    drained_data = drain(late)
    assert drained_data == ""
    eoi(late, split=False)


if __name__ == "__main__":
    main()
