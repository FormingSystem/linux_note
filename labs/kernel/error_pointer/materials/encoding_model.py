# 这里只模拟编码和比较，不创建、更不解引用任何指针。
errors = (-4096, -4095, -517, -22, -1, 0, 22)

for width in (32, 64):
    modulus = 1 << width
    mask = modulus - 1
    threshold = (-4095) & mask
    print(f"width={width}")
    for error in errors:
        encoded = error & mask
        detected = encoded >= threshold
        print(f"input={error:5d} value=0x{encoded:0{width // 4}x} error={int(detected)}")
    # 穷举所有允许的负错误值，检查编码后仍在区间内。
    for error in range(-4095, 0):
        encoded = error & mask
        assert encoded >= threshold
        assert encoded - modulus == error
