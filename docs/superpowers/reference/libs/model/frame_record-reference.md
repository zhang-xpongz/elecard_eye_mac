# `model::FrameRecord` / `model::SliceType` — 设计说明

> 纯 POD,无实现可言。本文解释设计取舍,不需要写代码。

---

## 1. 角色

`FrameRecord` 是 Phase C 的领域核心:StreamSession 把解析结果(NAL + 语法树 +
POC)聚成一帧一条记录,UI 帧列表、Hex 定位、解码预览都从它取数据。它是 parser
层与 UI 层之间的"中间产物"。

## 2. 字段取舍

| 字段 | 来源 | 用途 |
|---|---|---|
| index | 解码顺序计数 | 帧列表第一列 |
| poc | POCCalculator | 显示顺序列;后续按 poc 排序可还原播放序 |
| type | slice_type % 5 → SliceType | 帧列表 Type 列(I/P/B) |
| byte_offset_in_es / byte_size | NALSplitter 偏移 | Hex 定位、Size 列 |
| nals | NALSplitter | 解码时取字节、调试 |
| syntax_tree | parseSliceHeader | 右侧语法树面板 |
| missing_paramset | slice 解析容错 | UI 黄色提示 |

## 3. 为什么 SliceType 用 % 5 归一

H.264 slice_type 取值 0..9,5..9 只是"保证整图同型"的标注,语义上和 0..4 一样。
UI 只关心 I/P/B/SP/SI 五类,所以归一化。注意枚举值刻意对齐归一后的数字
(P=0,B=1,I=2,SP=3,SI=4),映射时直接 `static_cast<SliceType>(slice_type % 5)`。

## 4. 领域纯净性(DDD 约束)

`FrameRecord` 只 `#include` parser:: 头和 std,**绝不**碰 Qt / FFmpeg。这是
domain 层独立可测的前提(见 [[feedback-ddd-tdd]])。解码出的像素(`DecodedFrame`)
是另一层(libs/decoder)的事,不塞进 FrameRecord,避免 model 依赖 FFmpeg。

复用 parser:: 的类型(NALUnit / SyntaxNode / ParameterSetStore)而非在 model 重新
定义,是 plan §B.6 的决策:这些"已解析结构"归 parser 层,model 直接引用。

## 5. 后续扩展(MVP 不做)

- `std::vector<DecodedFrame>` 缩略图缓存 —— 放别处,不进 FrameRecord
- GOP 归属、参考关系(画 GOP 时间轴用)
- 多 slice 帧的完整 slice 列表(当前 MVP 一帧一 slice)
