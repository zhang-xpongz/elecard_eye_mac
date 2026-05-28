# `parser::stripEmulationPrevention` — 参考实现

> ⚠️ 自己实现完测试全绿后再看。

---

## 1. 背景:为什么需要 EP3

H.264 比特流里,起始码是 `00 00 01`。如果 RBSP 数据本身碰巧出现 `00 00 00/01/02/03`,解码器会误判成起始码。为避免这种"假起始码",编码器在 `00 00` 后插一个 `0x03`(emulation_prevention_three_byte):

```
RBSP:  ... 00 00 01 ...        (危险:像起始码)
EBSP:  ... 00 00 03 01 ...     (安全:插了 03)
```

NALSplitter 处理的是 EBSP(带 03),而 SPS/PPS parser 要解的是 RBSP(不带 03)。本函数就是这道还原工序。

## 2. 完整参考实现

```cpp
// libs/parser/src/ep3_strip.cpp
#include "parser/ep3_strip.h"

namespace parser {

std::vector<uint8_t> stripEmulationPrevention(const uint8_t* data, size_t size) {
    std::vector<uint8_t> out;
    if (data == nullptr || size == 0) return out;
    out.reserve(size);

    size_t i = 0;
    while (i < size) {
        if (i + 2 < size && data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x03) {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 3;                 // 丢弃 0x03,从其后继续
        } else {
            out.push_back(data[i]);
            ++i;
        }
    }
    return out;
}

}  // namespace parser
```

## 3. 逐测试走查

| 测试 | 输入 | 输出 | 说明 |
|---|---|---|---|
| NoEscapesUnchanged | `67 42 C0 1F` | `67 42 C0 1F` | 无 `00 00 03`,原样拷 |
| SingleEscapeRemoved | `67 00 00 03 01` | `67 00 00 01` | 删中间那个 03 |
| MultipleEscapes | `00 00 03 00 00 00 03 01` | `00 00 00 00 00 01` | 两处 EP3 各删一个 03 |
| EscapeAtEnd | `FF 00 00 03` | `FF 00 00` | 末尾 03 也剥 |
| EmptyInput | nullptr,0 | (空) | 早返回 |

`MultipleEscapes` 详细 trace:

```
i=0: 00 00 03 → push 00,00; i=3
i=3: data[3]=00, data[4]=00, data[5]=00(≠03)→ push 00; i=4
i=4: 00 00 03 → push 00,00; i=7
i=7: data[7]=01,后面不足 → push 01; i=8
结果: 00 00 00 00 00 01 ✓
```

## 4. 容易踩的坑

1. **边界 `i + 2 < size`**  
   要保证 `data[i+2]` 可读。`i + 2 < size` ⟺ `i+2 ≤ size-1`,合法。注意末尾 `00 00 03`(03 是最后一个 byte)时 i 指向第一个 00,i+2 指向 03,`i+2 < size` 成立 → 正确剥离。

2. **从 03 之后继续,不要回头**  
   剥掉 03 后 `i += 3`,落在 03 的下一字节。**不要**把保留的 `00 00` 再拿去和后续字节凑 `00 00 03`(spec 不这么做)。我们的 `i += 3` 天然避免了这点。

3. **不要在这里处理起始码**  
   本函数只认 `00 00 03`,不认 `00 00 01`。输入必须是**单个 NAL 的 payload**(NALSplitter 已经按起始码切好),不能把整段含起始码的流喂进来。

4. **别和 NALSplitter 的职责混淆**  
   NALSplitter:切边界,认 `00 00 01`。
   ep3_strip:还原 RBSP,认 `00 00 03`。
   两者扫的是不同的 3-byte 模式,不要合并。

## 5. 性能备注(MVP 不优化)

- 每次调用都新分配一个 vector。对 SPS/PPS 这种几十字节的小 NAL 无所谓。
- 大的 slice NAL(几十 KB)频繁调用时,可考虑复用缓冲或 in-place(若允许改输入)。MVP 阶段优先清晰,不优化。
