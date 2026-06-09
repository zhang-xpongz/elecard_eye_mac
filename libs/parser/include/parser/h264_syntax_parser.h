#pragma once

#include "parser/i_syntax_parser.h"

namespace parser {

// =============================================================================
// H264SyntaxParser — H.264 NAL 语法解析器(MVP:仅 SPS)
// =============================================================================
//
// 实现 ISyntaxParser。Phase B 逐步补齐:
//   B.4 parseSPS         ← 本任务
//   B.5 parsePPS
//   B.6 parseSliceHeader (需配合 ParameterSetStore 查 SPS/PPS)
//
// 语义遵循 H.264 (ITU-T H.264) §7.3。MVP 只解最小够用字段集,复杂可选项
// (完整 VUI、scaling list、FMO ...)暂不解析,用注释标注 future work。
//
class H264SyntaxParser : public ISyntaxParser {
public:
    // 见 ISyntaxParser::parseNAL。按 nal_unit_type 分派到对应 parseXxx。
    // 当前仅 type==7(SPS)有实现,其余返回空根节点。
    SyntaxNode parseNAL(const uint8_t* nal_data, size_t size,
                        uint8_t nal_unit_type) override;

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
};

}  // namespace parser
