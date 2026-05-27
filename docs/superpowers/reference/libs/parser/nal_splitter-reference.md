# `parser::NALSplitter` — 参考实现

> ⚠️ **不要在自己实现之前看这份文档**。自己写完测试全绿了再回来对照。

---

## 1. 设计要点

| 决策 | 理由 |
|---|---|
| **只切边界,不剥 EP3** | 单一职责。EP3 反转义是 RBSP 解析阶段的事(将在 Task B.3 的 `ep3_strip` 模块处理),Splitter 操作 EBSP |
| **byte_offset 含 leading 0** | 让原始字节区间(包含起始码)在 Hex viewer 上可以一字节不漏地高亮 |
| **不去 trailing_zero_8bits** | Annex-B 规范允许 NAL 末尾有 0 字节,如果它们之后接 `00 00 01`,我们的算法天然把第一个 0 归到下一个 NAL 的 4-byte 起始码 |
| **遇空/截断不报错,只返回空或部分** | 跟 `BitReader` 的"宽容解析"风格一致,domain 层错误用数据形态表达,不抛 |

## 2. 完整参考实现

```cpp
// libs/parser/src/nal_splitter.cpp
#include "parser/nal_splitter.h"

namespace parser {

std::vector<NALUnit> NALSplitter::split(const uint8_t* data, size_t size) {
    std::vector<NALUnit> result;
    if (data == nullptr || size < 3) return result;

    // Pass 1: 找到所有起始码的位置 (byte_offset, payload_offset)
    struct Start {
        size_t byte_offset;
        size_t payload_offset;
    };
    std::vector<Start> starts;

    for (size_t i = 0; i + 3 <= size; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
            Start s;
            s.payload_offset = i + 3;
            s.byte_offset = (i > 0 && data[i - 1] == 0x00) ? (i - 1) : i;
            starts.push_back(s);
            i += 2;   // 跳过 00 01 防止重复匹配(再 ++ 一次到 i+3)
        }
    }

    // Pass 2: 填 size + nal_unit_type
    result.reserve(starts.size());
    for (size_t k = 0; k < starts.size(); ++k) {
        NALUnit n;
        n.byte_offset    = starts[k].byte_offset;
        n.payload_offset = starts[k].payload_offset;
        n.size = (k + 1 < starts.size())
                   ? (starts[k + 1].byte_offset - starts[k].payload_offset)
                   : (size - starts[k].payload_offset);
        n.nal_unit_type = (n.payload_offset < size)
                            ? static_cast<uint8_t>(data[n.payload_offset] & 0x1F)
                            : 0;
        result.push_back(n);
    }

    return result;
}

}  // namespace parser
```

## 3. 关键算法走查

### 起始码识别

```
                ┌─ 3-byte: byte_offset = i,     payload = i+3
                │
i 处匹配 00 00 01 ┤
                │
                └─ 4-byte: byte_offset = i-1,   payload = i+3
                  (仅当 i>0 且 data[i-1]==0x00)
```

### Size 推断

```
NAL[k].size = NAL[k+1].byte_offset - NAL[k].payload_offset   (k 非最后)
NAL[k].size = total_buffer_size   - NAL[k].payload_offset    (k 是最后)
```

注意:**包含**了 NAL[k] 末尾的 trailing zeros(如果它们后面又接了 3-byte 起始码,就被算到 NAL[k] 里;如果后面接的是 4-byte 起始码,那个 leading 0 算到 NAL[k+1])。这跟 H.264 spec 的 byte stream format 允许两种解读 — 我们选了"trailing 0 归当前 NAL,leading 0 归下一个 NAL"的简单方案。

### 为什么 `i += 2`

匹配成功后,起始码占了 `data[i..i+2]` 共 3 字节。下次外层 `++i` 会到 `i+1`,但这一定不会再匹配新起始码(因为 `data[i+1]=00, data[i+2]=01, data[i+3]=?`,中间是 01 不是 00)。所以我们大胆 `i += 2` 跳过两字节,加上外层 ++,总跳 3 字节,直接到下一个可能的起始位置 `i+3`。

## 4. 容易踩的坑

1. **循环上界 `i + 3 <= size` 还是 `i + 2 < size`**  
   两者等价(对 size_t)。但写成 `i + 2 < size` 在 `size == 0` 时会触发 `2 < 0` ... 等等 size_t 永远 ≥0。实际上两种都不会数组越界,因为我们已经早返回了 `size < 3` 的情况。

2. **4-byte 起始码的 leading-zero 检测**  
   只有 `i > 0` 才能查 `data[i-1]`。如果 i==0 直接当 3-byte 处理(byte_offset=0)— 没有"之前"那个 byte 可言。

3. **`nal_unit_type & 0x1F`**  
   NAL header byte 高 3 bit 是 `forbidden_zero_bit (1) + nal_ref_idc (2)`,低 5 bit 才是 type。**不要直接用 `payload[0]`**。

4. **Pass 1 + Pass 2 vs 单遍**  
   也可以单遍:维护"上一个 NAL"指针,匹配新起始码时回填上一个的 size,最后还要处理末尾那个。两种风格都行,但单遍代码更乱。学习项目优先可读。

## 5. 测试数据中的 NAL Header 字节解读

| 字节 | 二进制 | forbidden | nal_ref_idc | nal_unit_type | 含义 |
|---|---|---|---|---|---|
| 0x67 | `0110 0111` | 0 | 3 | 7 | SPS |
| 0x68 | `0110 1000` | 0 | 3 | 8 | PPS |
| 0x65 | `0110 0101` | 0 | 3 | 5 | IDR slice |
| 0x41 | `0100 0001` | 0 | 2 | 1 | non-IDR slice |
| 0x06 | `0000 0110` | 0 | 0 | 6 | SEI |

参考 H.264 spec Table 7-1。

## 6. 后续扩展(不在 MVP)

- **Iterator 接口**:`begin()/end()` 返回 NALUnit 迭代器,避免一次性 vector 分配 — 适合超大流
- **流式 split**:接收增量字节,边来边切(用于实时流分析,MVP 不需要)
- **3-byte 起始码计数统计**:某些规范化场景下要求所有起始码都是 4-byte
