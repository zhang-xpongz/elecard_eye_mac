# `model::POCCalculator` — 参考实现

> ⚠️ 自己实现完、5 个测试全绿后再看。

---

## 1. POC 是什么,为什么要算

POC(Picture Order Count)= 帧的**显示顺序**。解码顺序 ≠ 显示顺序:B 帧在解码上
排在它参考的未来帧之后,但显示时插在中间。bitstream 里每帧只带 `pic_order_cnt_lsb`
(低位,会循环回绕),完整 POC 的高位(MSB)要解码器自己根据相邻帧推断累加。

例(fixture 02,解码顺序):I(lsb0) P(lsb6) B(lsb2) B(lsb4) → POC 0,6,2,4。
B 帧 POC=2 小于前面 P 帧 POC=6,正说明它显示在更早位置。

## 2. 完整参考实现

```cpp
#include "model/poc_calculator.h"

namespace model {

POCCalculator::POCCalculator(int log2_max_pic_order_cnt_lsb)
    : max_poc_lsb_(1 << log2_max_pic_order_cnt_lsb) {}

int POCCalculator::compute(bool is_idr, int pic_order_cnt_lsb) {
    if (is_idr) {
        prev_msb_ = 0;
        prev_lsb_ = 0;
    }

    const int half = max_poc_lsb_ / 2;
    int msb;
    if (pic_order_cnt_lsb < prev_lsb_ &&
        (prev_lsb_ - pic_order_cnt_lsb) >= half) {
        msb = prev_msb_ + max_poc_lsb_;          // lsb 回绕,MSB 进位
    } else if (pic_order_cnt_lsb > prev_lsb_ &&
               (pic_order_cnt_lsb - prev_lsb_) > half) {
        msb = prev_msb_ - max_poc_lsb_;          // 反向(乱序回看),MSB 退位
    } else {
        msb = prev_msb_;
    }

    const int poc = msb + pic_order_cnt_lsb;
    prev_msb_ = msb;
    prev_lsb_ = pic_order_cnt_lsb;
    return poc;
}

}  // namespace model
```

## 3. 逐测试走查(MaxPicOrderCntLsb=16,half=8)

**IncrementingLSB**:0→2→4→6,每步差 ≤8 不回绕,msb 恒 0,poc=lsb。

**LSBWrapAroundIncreasesMSB**:
```
IDR 0:  reset, msb0, poc0,  prev=(0,0)
   8:   8>0 diff8 不>8, msb0, poc8,  prev=(0,8)
   14:  14>8 diff6,    msb0, poc14, prev=(0,14)
   2:   2<14 diff12>=8 → 回绕, msb=0+16=16, poc=18 ✓
```

**SecondIDRResetsState**:第二个 `compute(true,0)` 把 prev 归零 → poc 0,之后
`compute(false,4)` → poc 4。验证 IDR 重置不依赖历史。

**MatchesFixture02DisplayOrder**:0,6,2,4 —— 真实流的显示顺序,见 §1。

## 4. 容易踩的坑

1. **两个条件都要带"距离判定"**。不是单纯比 `lsb < prevLsb` 就进位,而要
   `(prevLsb - lsb) >= half`。否则正常的小幅倒退(B 帧)会被误判成回绕。

2. **`>=` vs `>`**。进位分支是 `>= half`,退位分支是 `> half`。spec 原文如此
   (8-5/8-6 两式),边界值落点不同,别写反或统一。

3. **IDR 先重置再算**。顺序不能反:先 `prev=0` 再走 msb 计算,这样 IDR 自身
   (通常 lsb=0)得 poc=0。

4. **MVP 每帧都更新 prev**。spec 严格版只在"参考帧且无 MMCO=5"时更新
   prevMsb/prevLsb。MVP 不解 dec_ref_pic_marking,简化为每帧都更新。对测试用的
   全参考帧序列结果一致;非参考 B 帧密集的流可能有偏差,留作后续。

## 5. 后续(本 Task 不做)

- POC type 1(delta-based)、type 2(= 2*frame_num,无 B 帧)
- 严格的 prev 更新条件(区分参考/非参考帧、MMCO=5)
- 场编码(field)的 top/bottom field order count
