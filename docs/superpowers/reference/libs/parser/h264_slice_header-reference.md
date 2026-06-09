# `parser::H264SyntaxParser::parseSliceHeader` — 参考实现

> ⚠️ 自己实现完、6 个 slice header 测试全绿后再看。

---

## 1. 为什么 slice header 需要上下文

slice header 里两个字段是**变长**的,位宽不写在 slice 里,而在它引用的 SPS:

- `frame_num` 宽度 = `log2_max_frame_num_minus4 + 4`(bit)
- `pic_order_cnt_lsb` 宽度 = `log2_max_pic_order_cnt_lsb_minus4 + 4`(bit)

引用链:slice 的 `pic_parameter_set_id` → 查 PPS → PPS 的 `seq_parameter_set_id`
→ 查 SPS。所以解 slice 前必须先把 SPS/PPS 解析好放进 `ParameterSetStore` 并
`setParameterSets` 注入。查不到就优雅止损(incomplete),不能瞎猜位宽。

这是第一个"组合解析":BitReader + ep3_strip + SPS/PPS parser + ParameterSetStore
全用上了。

## 2. 完整参考实现

```cpp
SyntaxNode H264SyntaxParser::parseSliceHeader(const uint8_t* nal_data, size_t size,
                                              uint8_t nal_unit_type) {
    SyntaxNode root{"slice_header", "", 0, 0, false, {}};
    if (nal_data == nullptr || size < 1) { root.incomplete = true; return root; }

    BitReader r(nal_data, size);
    r.readBits(8);  // 跳过 NAL header

    addField(root, "first_mb_in_slice", r, ue());

    {   // slice_type 带可读后缀,手写(addField 只能写纯数字)
        const size_t start = r.bitOffset();
        const int st = static_cast<int>(r.readUE());
        std::string val = std::to_string(st) + " (" + sliceTypeLetter(st) + ")";
        root.children.push_back(SyntaxNode{
            "slice_type", val, start, r.bitOffset() - start, false, {}});
    }

    addField(root, "pic_parameter_set_id", r, ue());
    const int pps_id = std::stoi(root.children.back().value);

    // 回查上下文
    const SyntaxNode* pps = psets_ ? psets_->findPPS(pps_id) : nullptr;
    const SyntaxNode* sps = nullptr;
    if (pps) {
        const int sps_id = findIntField(*pps, "seq_parameter_set_id", -1);
        sps = psets_->findSPS(sps_id);
    }
    if (sps == nullptr) {        // 没上下文 → 解不下去
        root.incomplete = true;
        return root;
    }

    const int log2_fn  = findIntField(*sps, "log2_max_frame_num_minus4", 0);
    const int fmo_only = findIntField(*sps, "frame_mbs_only_flag", 1);
    const int poc_type = findIntField(*sps, "pic_order_cnt_type", 0);
    const int log2_poc = findIntField(*sps, "log2_max_pic_order_cnt_lsb_minus4", 0);

    const int fn_bits = log2_fn + 4;
    addField(root, "frame_num", r, [fn_bits](BitReader& br){ return br.readBits(fn_bits); });

    if (fmo_only == 0) {
        addField(root, "field_pic_flag", r, u(1));
        if (std::stoi(root.children.back().value) != 0)
            addField(root, "bottom_field_flag", r, u(1));
    }

    if (nal_unit_type == 5)      // IdrPicFlag
        addField(root, "idr_pic_id", r, ue());

    if (poc_type == 0) {
        const int poc_bits = log2_poc + 4;
        addField(root, "pic_order_cnt_lsb", r,
                 [poc_bits](BitReader& br){ return br.readBits(poc_bits); });
        // delta_pic_order_cnt_bottom(需 bottom_field_pic_order_in_frame_present_flag)
        // 及之后所有字段属 MVP 外,停在这里。
    }

    if (r.hasError()) root.incomplete = true;
    return root;
}
```

`u(n)` / `ue()` / `addField` / `findIntField` / `sliceTypeLetter` 都已在文件里
(前者两个 helper 在 anon namespace,后两个本任务新增)。

## 3. fixture 期望值对照

| | 01 IDR | 02 IDR | 02 P | 02 B |
|---|---|---|---|---|
| profile / poc_type | 66 / **2** | 77 / 0 | 77 / 0 | 77 / 0 |
| nal_unit_type | 5 | 5 | 1 | 1 |
| first_mb_in_slice | 0 | 0 | 0 | 0 |
| slice_type | 7 (I) | 7 (I) | 5 (P) | 6 (B) |
| frame_num | 0 | 0 | 1 | 2 |
| idr_pic_id | 0 | 0 | (无) | (无) |
| pic_order_cnt_lsb | **(无,type2)** | 0 | 6 | 2 |

测试覆盖:I/P/B 三种 slice、IDR vs 非 IDR(idr_pic_id 有无)、POC type 0 vs 2
(pic_order_cnt_lsb 有无)、以及"无参数集 → incomplete"的容错路径。

## 4. 容易踩的坑

1. **slice_type 不能用 addField**。addField 把值写成纯数字 "7",但测试要 "7 (I)"。
   必须手写节点,value 拼 `to_string(st) + " (" + sliceTypeLetter(st) + ")"`。

2. **slice_type 的字母看 `% 5`**。7 不是越界,而是"整图都是 I"。7%5=2=I,
   6%5=1=B,5%5=0=P。别漏 `% 5`。

3. **frame_num / pic_order_cnt_lsb 是 u(v),v 来自 SPS**。lambda 要捕获算好的
   位宽 `[fn_bits]`。fixture 都是 log2_*=0 / 2,即 u(4) / u(6);写死 4/6 在别的流
   会错,务必从 SPS 取。

4. **IdrPicFlag = (nal_unit_type == 5)**。只有 IDR slice 才有 idr_pic_id。P/B
   (type 1)没有,多读会错位。

5. **缺上下文要止损不要崩**。psets_==nullptr 或查不到 SPS 时置 incomplete 返回,
   已解出的 first_mb / slice_type / pps_id 保留(测试 MissingParameterSet 断言
   这点)。这是宽容解析原则,见 [[feedback-no-cpp-exceptions]]。

6. **MVP 边界在 pic_order_cnt_lsb**。之后是 ref_pic_list_modification、
   pred_weight_table、dec_ref_pic_marking、slice_qp_delta 等一长串可变结构,MVP
   不解。不要试图硬读到 slice_qp_delta —— 中间结构没解会全错位。

## 5. 字段语义速记

- `first_mb_in_slice`:本 slice 第一个宏块的地址。0 表示一帧的起始 slice(后续
  StreamSession 用它判断"新帧 vs 同帧的后续 slice")。
- `slice_type`:决定预测方式(I 帧内 / P 单向 / B 双向)。UI 帧列表的 Type 列。
- `frame_num`:参考帧管理用的循环计数,IDR 处归零。
- `pic_order_cnt_lsb`:POC 计算的低位(Phase C 的 POCCalculator 用它推显示顺序)。
- `idr_pic_id`:区分相邻 IDR,容错用。

## 6. 后续(本 Task 不做)

- ref_pic_list_modification / pred_weight_table / dec_ref_pic_marking
- slice_qp_delta、disable_deblocking_filter_idc(去块滤波)
- POC type 1 / 2 的 delta_pic_order_cnt 字段
- 把 slice_type 映射到 model::SliceType 枚举(Phase C 的 StreamSession 做)
