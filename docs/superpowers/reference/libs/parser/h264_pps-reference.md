# `parser::H264SyntaxParser::parsePPS` — 参考实现

> ⚠️ 自己实现完、3 个 PPS 测试全绿后再看。

---

## 1. 与 SPS 的异同

结构与 parseSPS 完全同构(含 header → readBits(8) 跳过 → 逐字段 addField),复用
同一个 `addField` helper。两点新东西:

- **首次用 `readSE`**(有符号 Exp-Golomb):`pic_init_qp_minus26`、
  `pic_init_qs_minus26`、`chroma_qp_index_offset` 都是 se(v),值可能为负。
  `addField` 里 `static_cast<long long>` + `std::to_string` 能正确显示 "-3"。
- **`weighted_bipred_idc` 是 u(2)**(2 bit),别写成 u(1)。

## 2. 完整参考实现

```cpp
SyntaxNode H264SyntaxParser::parsePPS(const uint8_t* nal_data, size_t size) {
    SyntaxNode root{"pic_parameter_set_rbsp", "", 0, 0, false, {}};
    if (nal_data == nullptr || size < 1) return root;

    BitReader r(nal_data, size);
    r.readBits(8);  // 跳过 NAL header

    addField(root, "pic_parameter_set_id", r, ue());
    addField(root, "seq_parameter_set_id", r, ue());
    addField(root, "entropy_coding_mode_flag", r, u(1));
    addField(root, "bottom_field_pic_order_in_frame_present_flag", r, u(1));

    addField(root, "num_slice_groups_minus1", r, ue());
    if (lastVal(root) != 0) {          // FMO slice group map,MVP 不解
        root.incomplete = true;
        return root;
    }

    addField(root, "num_ref_idx_l0_default_active_minus1", r, ue());
    addField(root, "num_ref_idx_l1_default_active_minus1", r, ue());
    addField(root, "weighted_pred_flag",  r, u(1));
    addField(root, "weighted_bipred_idc", r, u(2));   // 2 bit

    addField(root, "pic_init_qp_minus26",    r, se());  // 可负
    addField(root, "pic_init_qs_minus26",    r, se());
    addField(root, "chroma_qp_index_offset", r, se());  // 可负

    addField(root, "deblocking_filter_control_present_flag", r, u(1));
    addField(root, "constrained_intra_pred_flag",            r, u(1));
    addField(root, "redundant_pic_cnt_present_flag",         r, u(1));
    // more_rbsp_data() 时还有 PPS 扩展(transform_8x8_mode_flag 等),MVP 不解。

    if (r.hasError()) root.incomplete = true;
    return root;
}
```

> 其中 `u(n)` / `ue()` / `se()` / `lastVal` 与 parseSPS reference 里定义的 helper 相同
> (都在 anon namespace)。`se()` 写作 `[](BitReader& r){ return r.readSE(); }`。

## 3. 两个 fixture 字段对照

| 字段 | fixture01 Baseline | fixture03 High |
|---|---|---|
| pic_parameter_set_id | 0 | 0 |
| seq_parameter_set_id | 0 | 0 |
| entropy_coding_mode_flag | 0 (CAVLC) | 1 (CABAC) |
| num_ref_idx_l0_default_active_minus1 | 0 | 3 |
| weighted_pred_flag | 0 | 1 |
| weighted_bipred_idc | 0 | 2 |
| pic_init_qp_minus26 | -3 | -3 |
| pic_init_qs_minus26 | 0 | 0 |
| chroma_qp_index_offset | -2 | -2 |
| deblocking_filter_control_present_flag | 1 | 1 |
| redundant_pic_cnt_present_flag | 0 | 0 |
| 解析后 more_rbsp_data | 否 | **是**(PPS 扩展,MVP 忽略) |

fixture01 走 CAVLC 路径,fixture03 走 CABAC + 多参考帧 + 加权预测路径,合起来覆盖
MVP 全部字段。

## 4. 容易踩的坑

1. **`weighted_bipred_idc` 是 2 bit**。写成 u(1) 会少读 1 bit,后面 se 字段全错位,
   `pic_init_qp_minus26` 解出垃圾值(可能很大或越界 → incomplete)。

2. **se 不是 ue**。三个 qp 字段必须 `readSE`。用 ue 读 "-3" 会得到别的非负数,
   测试断言 "-3" 直接失败。回忆 zigzag:codeNum=5 → se=-3,codeNum=3 → se=-2。

3. **FMO 提前止损**。`num_slice_groups_minus1>0` 时后面是 slice_group_map,字段集
   复杂且 MVP 不需要 → `incomplete=true; return`。我们 fixture 都是 0,不触发,但
   真实流(尤其老旧会议流)可能有。

4. **不要试图解 PPS 扩展**。fixture03 解完 MVP 字段后 more_rbsp_data 为真(还有
   transform_8x8_mode_flag / pic_scaling_matrix_present_flag /
   second_chroma_qp_index_offset)。MVP 在 redundant_pic_cnt_present_flag 后停手,
   不读这些。测试不会断言它们。

## 5. 字段语义速记

- `entropy_coding_mode_flag`:0=CAVLC,1=CABAC。决定 slice data 的熵编码方式
  (slice data 解析 MVP 不做,但这是关键元信息)。
- `pic_init_qp_minus26`:初始量化参数 QP = 26 + 该值。-3 → QP=23。
- `num_ref_idx_l0/l1_default_active_minus1`:默认参考帧数 - 1,slice header 可覆盖。
- `weighted_pred_flag` / `weighted_bipred_idc`:P/B slice 加权预测开关。
- `deblocking_filter_control_present_flag`:slice header 是否带去块滤波控制字段
  (影响 B.6 slice header 解析)。

## 6. 后续(本 Task 不做)

- PPS 扩展字段(transform_8x8_mode 等)
- FMO slice group map 解析
- 把 PPS 树存入 ParameterSetStore(B.6a),供 slice header 解析查 entropy_coding_mode
