"""按 GICv3.0 普通 SPI 的公开编程接口计算索引，不访问硬件。"""
import argparse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("intid", type=int)
    args = parser.parse_args()
    if not 32 <= args.intid <= 1019:
        parser.error("本例只计算 GICv3.0 普通 SPI：32～1019")
    intid = args.intid
    # 商选择寄存器，余数选择该寄存器里的字段。
    enable_index, enable_bit = divmod(intid, 32)
    config_index, config_slot = divmod(intid, 16)
    priority_index, priority_slot = divmod(intid, 4)
    print(f"使能: GICD_ISENABLER{enable_index}, bit {enable_bit}")
    print(f"触发: GICD_ICFGR{config_index}, "
          f"bits {2 * config_slot + 1}:{2 * config_slot}")
    print(f"优先级: 字节 {intid}; GICD_IPRIORITYR{priority_index}, "
          f"bits {8 * priority_slot + 7}:{8 * priority_slot}")
    print(f"目标: GICD_IROUTER{intid}")


if __name__ == "__main__":
    main()
