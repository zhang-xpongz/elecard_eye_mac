#pragma once

#include "parser/i_syntax_parser.h"
#include "parser/parameter_set_store.h"

namespace parser {

// =============================================================================
// H264SyntaxParser — H.264 NAL 语法解析器(MVP:SPS / PPS / Slice Header)
// =============================================================================
//
// 实现 ISyntaxParser。Phase B 逐步补齐:
//   B.4 parseSPS         ✓
//   B.5 parsePPS         ✓
//   B.6 parseSliceHeader ← 本任务(需配合 ParameterSetStore 查 SPS/PPS)
//
// 语义遵循 H.264 (ITU-T H.264) §7.3。MVP 只解最小够用字段集,复杂可选项
// (完整 VUI、scaling list、FMO ...)暂不解析,用注释标注 future work。
//
class H264SyntaxParser : public ISyntaxParser {
public:
    // 见 ISyntaxParser::parseNAL。按 nal_unit_type 分派:
    //   7→parseSPS,8→parsePPS,1/5→parseSliceHeader,其余返回空根节点。
    SyntaxNode parseNAL(const uint8_t* nal_data, size_t size,
                        uint8_t nal_unit_type) override;

    // -------------------------------------------------------------------------
    // setParameterSets — 注入 SPS/PPS 仓储,供 slice header 解析回查上下文
    //
    // slice header 的 frame_num / pic_order_cnt_lsb 位宽取自所引用 SPS,故解析
    // slice 前必须先 set。传入指针由调用方持有,本类只读、不拥有,生命周期须覆盖
    // 后续所有 parseSliceHeader 调用。未注入(nullptr)时 slice 解析只能解出
    // pic_parameter_set_id 之前的字段,然后置 incomplete 返回。
    void setParameterSets(const ParameterSetStore* store) { psets_ = store; }

private:
    // -------------------------------------------------------------------------
    // parseSPS — 解析 seq_parameter_set_rbsp(H.264 §7.3.2.1.1)
    //
    // 输入:nal_data 含 NAL header 的 SPS 字节(EP3 已剥),size 字节数。
    // 输出:根节点 name="seq_parameter_set_rbsp",所有字段作为**扁平子节点**
    //       (不嵌套),便于 find()/Hex 联动。各子节点:
    //         name        = 规范字段名(如 "profile_idc")
    //         value       = 十进制字符串(枚举可带可读后缀)
    //         bit_offset  = 该字段在 nal_data 内的起始 bit(profile_idc 从 8 起)
    //         bit_length  = 该字段消耗的 bit 数
    //
    // MVP 字段集(条件字段按 §7.3.2.1.1 的 if 分支决定是否出现):
    //   profile_idc, constraint_set_flags(含 reserved 共 8 bit 合成一个节点),
    //   level_idc, seq_parameter_set_id,
    //   [High profile 才有] chroma_format_idc, bit_depth_luma_minus8,
    //       bit_depth_chroma_minus8, qpprime_y_zero_transform_bypass_flag,
    //   log2_max_frame_num_minus4, pic_order_cnt_type,
    //   [pic_order_cnt_type==0 才有] log2_max_pic_order_cnt_lsb_minus4,
    //   max_num_ref_frames, gaps_in_frame_num_value_allowed_flag,
    //   pic_width_in_mbs_minus1, pic_height_in_map_units_minus1,
    //   frame_mbs_only_flag, direct_8x8_inference_flag,
    //   frame_cropping_flag, [若为 1] crop_{left,right,top,bottom}_offset,
    //   vui_parameters_present_flag。
    //
    // NOT PARSED(MVP 外,遇到时不再深入,见 reference doc):
    //   chroma_format_idc==3 的 separate_colour_plane_flag、
    //   seq_scaling_matrix_present_flag==1 的 scaling list、
    //   完整 VUI、pic_order_cnt_type==1 的相关字段。
    //
    // 错误处理:任一字段读取使 BitReader.hasError()==true 时,把根节点
    // incomplete 置 true 并停止,不抛异常。
    SyntaxNode parseSPS(const uint8_t* nal_data, size_t size);

    // -------------------------------------------------------------------------
    // parsePPS — 解析 pic_parameter_set_rbsp(H.264 §7.3.2.2)
    //
    // 输入 / 输出约定同 parseSPS:含 NAL header(EP3 已剥),根节点
    // name="pic_parameter_set_rbsp",字段作扁平子节点,pic_parameter_set_id
    // 从 bit 8 起。
    //
    // MVP 字段集:
    //   pic_parameter_set_id, seq_parameter_set_id, entropy_coding_mode_flag,
    //   bottom_field_pic_order_in_frame_present_flag, num_slice_groups_minus1,
    //   num_ref_idx_l0_default_active_minus1, num_ref_idx_l1_default_active_minus1,
    //   weighted_pred_flag, weighted_bipred_idc(u(2)),
    //   pic_init_qp_minus26(se), pic_init_qs_minus26(se),
    //   chroma_qp_index_offset(se), deblocking_filter_control_present_flag,
    //   constrained_intra_pred_flag, redundant_pic_cnt_present_flag。
    //
    // NOT PARSED(MVP 外):
    //   num_slice_groups_minus1>0 的 FMO slice group map(遇到则置 incomplete);
    //   redundant_pic_cnt_present_flag 之后的 PPS 扩展(transform_8x8_mode_flag、
    //   pic_scaling_matrix、second_chroma_qp_index_offset)—— High profile 流里
    //   存在(more_rbsp_data),MVP 读到 redundant_pic_cnt_present_flag 即停。
    //
    // 错误处理:同 parseSPS,出错置根节点 incomplete,不抛异常。
    SyntaxNode parsePPS(const uint8_t* nal_data, size_t size);

    // -------------------------------------------------------------------------
    // parseSliceHeader — 解析 slice_header(H.264 §7.3.3)
    //
    // 输入:nal_data 含 NAL header 的 slice 字节(EP3 已剥),size 字节数,
    //       nal_unit_type(1=非 IDR slice,5=IDR slice)。IdrPicFlag = (type==5)。
    // 输出:根节点 name="slice_header",字段作扁平子节点,first_mb_in_slice 从
    //       bit 8 起。
    //
    // 上下文依赖(关键):frame_num 位宽 = SPS.log2_max_frame_num_minus4 + 4;
    //   pic_order_cnt_lsb 位宽 = SPS.log2_max_pic_order_cnt_lsb_minus4 + 4。
    //   通过 pic_parameter_set_id → PPS → seq_parameter_set_id → SPS 这条链,
    //   从注入的 ParameterSetStore 回查。查不到(未 setParameterSets 或 store 里
    //   没有对应 id)时,置根节点 incomplete 并返回已解出的前缀字段。
    //
    // MVP 字段集(按 §7.3.3 顺序,条件字段按 if 分支决定是否出现):
    //   first_mb_in_slice, slice_type(value 带可读后缀如 "7 (I)"),
    //   pic_parameter_set_id, frame_num(u(v)),
    //   [!frame_mbs_only_flag] field_pic_flag [+ bottom_field_flag],
    //   [IdrPicFlag] idr_pic_id,
    //   [pic_order_cnt_type==0] pic_order_cnt_lsb(u(v))。
    //
    // NOT PARSED(MVP 边界,解到 pic_order_cnt_lsb 即停):
    //   delta_pic_order_cnt_bottom、ref_pic_list_modification、pred_weight_table、
    //   dec_ref_pic_marking、slice_qp_delta、disable_deblocking_filter_idc 等。
    //   这些需要先解上面若干复杂可变长结构,留待 MVP 之后迭代。
    //
    // slice_type 取值(§7.4.3):0/5=P,1/6=B,2/7=I,3/8=SP,4/9=SI。
    //   value 串格式 "<raw> (<letter>)",letter 由 raw % 5 决定。
    SyntaxNode parseSliceHeader(const uint8_t* nal_data, size_t size,
                                uint8_t nal_unit_type);

    // SPS/PPS 仓储,只读不拥有(见 setParameterSets)。
    const ParameterSetStore* psets_ = nullptr;
};

}  // namespace parser
