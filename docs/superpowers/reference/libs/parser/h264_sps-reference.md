# `parser::H264SyntaxParser::parseSPS` — 参考实现

> ⚠️ 自己实现完、3 个 SPS 测试全绿后再看。

---

## 1. 数据流位置

```
ES bytes ──NALSplitter──▶ NALUnit(payload 含 header)
        ──stripEmulationPrevention──▶ RBSP(EP3 已剥,仍含 header byte)
        ──parseNAL(rbsp, size, 7)──▶ parseSPS ──▶ SyntaxNode 树
```

约定:`parseSPS` 收到的 buffer **含 1 字节 NAL header**。所以先 `readBits(8)` 跳过
header,`profile_idc` 才从 bit 8 开始 —— 这正是 `FieldsAreContiguousFromBit8` 断言的。

> 关于 bit_offset 与 EP3 的细节:我们先剥 EP3 再解析,所以 bit_offset 是相对
> **剥离后**缓冲区的。Hex viewer 要高亮原始 ES 字节时,需要把剥掉的 0x03 数量
> 映射回去。MVP 的两个 fixture 的 EP3 都在被断言字段**之后**,不影响测试。完整
> 的 offset 回映射留到 Hex 联动阶段(Phase E)处理。

## 2. 完整参考实现

```cpp
#include "parser/h264_syntax_parser.h"

#include <string>

#include "parser/bit_reader.h"

namespace parser {

namespace {

template <typename ReadFn>
void addField(SyntaxNode& parent, const char* name, BitReader& r, ReadFn read_fn) {
    const size_t start = r.bitOffset();
    const long long v = static_cast<long long>(read_fn(r));
    parent.children.push_back(SyntaxNode{
        name, std::to_string(v), start, r.bitOffset() - start, false, {}});
}

bool isHighProfile(int profile_idc) {
    switch (profile_idc) {
        case 100: case 110: case 122: case 244:
        case 44:  case 83:  case 86:  case 118:
        case 128: case 138: case 139: case 134: case 135:
            return true;
        default:
            return false;
    }
}

// 把 BitReader 成员函数包成 read_fn,避免每处写 lambda。
auto u(int n)  { return [n](BitReader& r) { return r.readBits(n); }; }
auto ue()      { return    [](BitReader& r) { return r.readUE();  }; }

int lastVal(const SyntaxNode& root) {
    return std::stoi(root.children.back().value);
}

}  // namespace

SyntaxNode H264SyntaxParser::parseNAL(const uint8_t* nal_data, size_t size,
                                      uint8_t nal_unit_type) {
    if (nal_unit_type == 7) return parseSPS(nal_data, size);
    return SyntaxNode{};
}

SyntaxNode H264SyntaxParser::parseSPS(const uint8_t* nal_data, size_t size) {
    SyntaxNode root{"seq_parameter_set_rbsp", "", 0, 0, false, {}};
    BitReader r(nal_data, size);
    r.readBits(8);  // 跳过 NAL header → profile_idc 从 bit 8

    addField(root, "profile_idc",          r, u(8));
    const int profile = lastVal(root);
    addField(root, "constraint_set_flags", r, u(8));   // 6 flag + 2 reserved
    addField(root, "level_idc",            r, u(8));
    addField(root, "seq_parameter_set_id", r, ue());

    if (isHighProfile(profile)) {
        addField(root, "chroma_format_idc", r, ue());
        // chroma_format_idc==3 的 separate_colour_plane_flag:MVP 外,不解。
        addField(root, "bit_depth_luma_minus8",   r, ue());
        addField(root, "bit_depth_chroma_minus8", r, ue());
        addField(root, "qpprime_y_zero_transform_bypass_flag", r, u(1));
        addField(root, "seq_scaling_matrix_present_flag",      r, u(1));
        if (lastVal(root) != 0) {           // scaling list 不在 MVP 范围
            root.incomplete = true;
            return root;
        }
    }

    addField(root, "log2_max_frame_num_minus4", r, ue());
    addField(root, "pic_order_cnt_type",        r, ue());
    const int poc_type = lastVal(root);
    if (poc_type == 0) {
        addField(root, "log2_max_pic_order_cnt_lsb_minus4", r, ue());
    } else if (poc_type == 1) {
        // pic_order_cnt_type==1 的字段集 MVP 不解。
        root.incomplete = true;
        return root;
    }

    addField(root, "max_num_ref_frames",                   r, ue());
    addField(root, "gaps_in_frame_num_value_allowed_flag", r, u(1));
    addField(root, "pic_width_in_mbs_minus1",              r, ue());
    addField(root, "pic_height_in_map_units_minus1",       r, ue());
    addField(root, "frame_mbs_only_flag",                  r, u(1));
    if (lastVal(root) == 0) {
        addField(root, "mb_adaptive_frame_field_flag", r, u(1));
    }
    addField(root, "direct_8x8_inference_flag", r, u(1));

    addField(root, "frame_cropping_flag", r, u(1));
    if (lastVal(root) != 0) {
        addField(root, "frame_crop_left_offset",   r, ue());
        addField(root, "frame_crop_right_offset",  r, ue());
        addField(root, "frame_crop_top_offset",    r, ue());
        addField(root, "frame_crop_bottom_offset", r, ue());
    }
    addField(root, "vui_parameters_present_flag", r, u(1));  // VUI 本体不解

    if (r.hasError()) root.incomplete = true;
    return root;
}

}  // namespace parser
```

## 3. 两个 fixture 的字段对照

| 字段 | fixture01 Baseline | fixture03 High |
|---|---|---|
| profile_idc | 66 | 100 |
| level_idc | 10 | 10 |
| seq_parameter_set_id | 0 | 0 |
| chroma_format_idc | (无) | 1 |
| bit_depth_luma_minus8 | (无) | 0 |
| pic_order_cnt_type | 2 | 0 |
| log2_max_pic_order_cnt_lsb_minus4 | (无,type≠0) | 2 |
| max_num_ref_frames | 0 | 4 |
| pic_width_in_mbs_minus1 | 7 | 7 |
| pic_height_in_map_units_minus1 | 7 | 7 |
| frame_mbs_only_flag | 1 | 1 |
| frame_cropping_flag | 0 | 0 |
| vui_parameters_present_flag | 1 | 1 |
| 分辨率 | 128×128 | 128×128 |

两个 fixture 故意选了不同分支:Baseline 走"非 High + POC type 2",High 走
"High block + POC type 0",合起来覆盖了 MVP 全部条件路径。

## 4. 容易踩的坑

1. **header byte 必须跳过**。忘了 `readBits(8)` 的话,会把 0x67 当成 profile_idc,
   且所有 bit_offset 偏 8,`FieldsAreContiguousFromBit8` 直接挂。

2. **constraint flags 合成一个节点**。规范里是 6 个独立 flag + 2 个 reserved bit,
   MVP 为简洁用一次 `readBits(8)` 当一个 8-bit 字段。要拆成 6 个 flag 也行,但要
   保证它们 bit_offset 连续(否则 contiguity 测试失败)。

3. **条件字段顺序不能错**。High block 在 `seq_parameter_set_id` 之后、
   `log2_max_frame_num_minus4` 之前。POC lsb 紧跟 `pic_order_cnt_type`。顺序错了
   后续字段全错位,值对不上。

4. **遇到 MVP 外的可选项要"优雅止损"**:scaling matrix present / poc_type==1 时,
   置 `root.incomplete=true` 并 `return`,**不要**硬解(会读出垃圾值且越界)。这是
   宽容解析原则的体现(见 [[feedback-no-cpp-exceptions]])。我们的 fixture 不触发
   这些分支,但真实流可能有。

5. **`lastVal` 依赖"刚 push 的节点在 children.back()"**。每次 addField 后立刻取,
   不要插入别的 children 打断。

## 5. 字段语义速记(够 MVP 用)

- `pic_width_in_mbs_minus1`:宽度 = (值+1) × 16 像素。7 → 128。
- `pic_height_in_map_units_minus1`:高度 = (值+1) × 16 × (2 − frame_mbs_only_flag)。
  frame_mbs_only=1 时就是 (值+1)×16。7 → 128。
- `log2_max_frame_num_minus4`:frame_num 的位宽 = 值+4,影响 slice header 解析(B.6 用)。
- `pic_order_cnt_type` / `log2_max_pic_order_cnt_lsb_minus4`:POC 计算用(Phase C)。
- `max_num_ref_frames`:DPB 大小提示。
- `vui_parameters_present_flag`:VUI(帧率、宽高比等)是否存在;MVP 只记 flag 不解内容。

## 6. 后续(本 Task 不做)

- 拆 constraint flags 成 6 个 bool 节点(更细的 Hex 联动)
- 解 VUI 子集(timing_info → 帧率)
- scaling list 解析(High profile 自定义量化矩阵)
- bit_offset 的 EP3 回映射(Phase E Hex 联动时统一处理)
