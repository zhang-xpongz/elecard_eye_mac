# `parser::BitReader` — 参考实现

> ⚠️ **不要在自己实现之前看这份文档**。本文档是你跑通 6 个测试之后用来对照学习的参考答案。
> 推荐流程:看完测试 → 自己写 → 测试全绿 → 回来对照本文档。

---

## 1. 设计要点回顾

| 决策 | 理由 |
|---|---|
| **MSB-first** | H.264 比特流约定:每个字节的最高位是 bit-0,这与人类读 hex 的方向一致 |
| **错误标志(`error_`)而非异常** | 项目级硬约束:禁用 C++ 异常,跨库 ABI 友好,嵌入式风格习惯 |
| **n ∈ [1, 32]** | 用 `uint32_t` 作返回类型,32-bit 是天然上限。需要更大数值用两次 readBits 拼 |
| **Exp-Golomb 内置** | H.264 几乎所有 `ue(v)` / `se(v)` 字段都用得上,放进 BitReader 减少调用方记 cursor 的成本 |

## 2. 完整参考实现

```cpp
// libs/parser/src/bit_reader.cpp
#include "parser/bit_reader.h"

namespace parser {

BitReader::BitReader(const uint8_t* data, size_t size_bytes)
    : data_(data), size_bytes_(size_bytes) {}

uint32_t BitReader::readBits(int n) {
    // 越界检查:bit_offset_ + n 必须 ≤ 总 bit 数
    if (error_ || n <= 0 || n > 32 ||
        bit_offset_ + static_cast<size_t>(n) > size_bytes_ * 8) {
        error_ = true;
        return 0;
    }

    uint32_t value = 0;
    for (int i = 0; i < n; ++i) {
        const size_t byte_idx = bit_offset_ / 8;
        const int    bit_idx  = 7 - (bit_offset_ % 8);  // MSB-first
        const uint32_t bit    = (data_[byte_idx] >> bit_idx) & 1u;
        value = (value << 1) | bit;
        ++bit_offset_;
    }
    return value;
}

uint32_t BitReader::readUE() {
    // 数 leading zeros
    int leading_zeros = 0;
    while (!error_ && leading_zeros < 32) {
        const uint32_t b = readBits(1);
        if (error_) return 0;
        if (b == 1) break;
        ++leading_zeros;
    }
    if (leading_zeros >= 32) {  // 防御:32 个 0 还没看到 1 — 流坏了
        error_ = true;
        return 0;
    }

    // 读 leading_zeros 个 bit 作为 V
    uint32_t v = 0;
    if (leading_zeros > 0) {
        v = readBits(leading_zeros);
        if (error_) return 0;
    }

    // codeNum = 2^leading_zeros - 1 + V
    return (1u << leading_zeros) - 1u + v;
}

int32_t BitReader::readSE() {
    const uint32_t k = readUE();
    if (error_) return 0;
    // H.264 9.1.1: k=0→0, k=1→+1, k=2→-1, k=3→+2, k=4→-2 ...
    if (k & 1u) return  static_cast<int32_t>((k + 1u) >> 1);
    else        return -static_cast<int32_t>(k >> 1);
}

size_t BitReader::bitOffset() const { return bit_offset_; }
bool   BitReader::hasError() const  { return error_; }

}  // namespace parser
```

注意:实现完成后,把骨架里那行 `(void)data_; (void)size_bytes_;` 删掉。

## 3. Exp-Golomb 算法走查

### Unsigned (`ue(v)`,H.264 9.1)

编码格式:`(leading_zeros 个 0) + 1 + (leading_zeros 个 bit 的尾数 V)`

| codeNum | 码字 | 拆解 |
|---|---|---|
| 0 | `1` | 0 leading zeros,V=∅ |
| 1 | `010` | 1 leading zero,V=`0` |
| 2 | `011` | 1 leading zero,V=`1` |
| 3 | `00100` | 2 leading zeros,V=`00` |
| 4 | `00101` | 2 leading zeros,V=`01` |
| 5 | `00110` | 2 leading zeros,V=`10` |
| 6 | `00111` | 2 leading zeros,V=`11` |
| 7 | `0001000` | 3 leading zeros,V=`000` |

公式:`codeNum = 2^leading_zeros - 1 + V`

### Signed (`se(v)`,H.264 9.1.1)

先解出 ue(v) 得 `k`,再做 zigzag 映射:

| k(ue) | se |
|---|---|
| 0 | 0 |
| 1 | +1 |
| 2 | -1 |
| 3 | +2 |
| 4 | -2 |

公式:`se = (k 为奇) ? +(k+1)/2 : -k/2`

## 4. 容易踩的坑

1. **MSB-first vs LSB-first 搞反**  
   `(data_[byte] >> (7 - bit_in_byte)) & 1` 是 MSB-first,反过来是 LSB-first。FFmpeg 的 `get_bits` 也是 MSB-first。验证方法:测试 `ReadOneBitAtATime` 用 `0xB4 = 1011 0100`,第一个 bit 应该是 1。

2. **`readUE` 防御 32+ 个 0**  
   合法 H.264 流不会出现 32 个 leading zeros(否则 codeNum > 2^32-1 超 uint32 范围)。损坏流可能死循环 — 必须设上限。

3. **错误状态粘性**  
   一旦 `error_=true`,后续 `readBits` 应该立刻短路返回 0 而不是继续尝试读(否则破坏 `hasError()` 的语义)。

4. **`readBits(0)` 的语义**  
   按规范应返回 0 且不推进 cursor。参考实现里 `n <= 0` 直接置错;另一种合理设计是允许 `n=0` 静默返回 0。两种都行,但要在测试里固定一种。

## 5. 与其他实现的对比

| 实现 | 风格 |
|---|---|
| FFmpeg `get_bits.h` | C 宏 + uint64 cache,极致性能。可读性差 |
| h264bitstream | 一位一位读,清晰但慢。本参考接近这种风格 |
| JM 参考解码器 | `read_ue()` 等独立函数,无 reader 对象 |

本参考选了"一位一位读"的清晰风格 — 学习项目优先可读性。Phase B 跑通后如果想优化,可以加 64-bit cache。

## 6. 你可能想做的扩展(MVP 后)

- `peekBits(n)` — 不推进 cursor 的预读
- `skipBits(n)` — 不返回值,只推进
- `byteAligned()` — 当前是否对齐到字节边界(H.264 trailing bits 检查用)
- `readUEMax(max)` — 带上限的 ue 解析,超出立即置错

这些不在 MVP 范围内,接口可以后续平滑加。
