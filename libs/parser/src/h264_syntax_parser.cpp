#include "parser/h264_syntax_parser.h"

#include <climits>
#include <string>

#include "parser/bit_reader.h"

namespace parser {

namespace {

// 建议 helper:读一个字段并作为叶子节点追加到 parent.children。
//   - 记录读之前的 bitOffset 作为字段起点
//   - 调 read_fn(r) 读出值
//   - bit_length = 读之后的 bitOffset - 起点
// read_fn 形如 [](BitReader& r){ return r.readUE(); },返回值会被转成 value 字符串。
//
// 用法示例(实现时):
//   addField(root, "profile_idc", r, [](BitReader& br){ return br.readBits(8); });
template <typename ReadFn>
void addField(SyntaxNode& parent, const char* name, BitReader& r, ReadFn read_fn) {
    const size_t start = r.bitOffset();
    const long long v = static_cast<long long>(read_fn(r));
    parent.children.push_back(SyntaxNode{
        name, std::to_string(v), start, r.bitOffset() - start, false, {}});
}

// High / High 系列 profile,需多解 chroma_format_idc 等字段(§7.3.2.1.1)。
[[maybe_unused]] bool isHighProfile(int profile_idc) {
    switch (profile_idc) {
        case 100: case 110: case 122: case 244:
        case 44:  case 83:  case 86:  case 118:
        case 128: case 138: case 139: case 134: case 135:
            return true;
        default:
            return false;
    }
}

// 深度优先在已解析的 SPS/PPS 树里按 name 取整数字段;缺省返回 fallback。
[[maybe_unused]] int findIntField(const SyntaxNode& root, const char* name,
                                  int fallback) {
    if (root.name == name) return std::stoi(root.value);
    for (const auto& c : root.children) {
        const int v = findIntField(c, name, INT_MIN);
        if (v != INT_MIN) return v;
    }
    return fallback;
}

// slice_type 数值 → 可读字母(§7.4.3,5..9 表示"整图同型")。
[[maybe_unused]] const char* sliceTypeLetter(int slice_type) {
    switch (slice_type % 5) {
        case 0: return "P";
        case 1: return "B";
        case 2: return "I";
        case 3: return "SP";
        case 4: return "SI";
        default: return "?";
    }
}

}  // namespace

SyntaxNode H264SyntaxParser::parseNAL(const uint8_t* nal_data, size_t size,
                                      uint8_t nal_unit_type) {
    if (nal_unit_type == 7) return parseSPS(nal_data, size);
    if (nal_unit_type == 8) return parsePPS(nal_data, size);
    if (nal_unit_type == 1 || nal_unit_type == 5)
        return parseSliceHeader(nal_data, size, nal_unit_type);
    return SyntaxNode{};
}

SyntaxNode H264SyntaxParser::parseSPS(const uint8_t* nal_data, size_t size) {
    SyntaxNode root{"seq_parameter_set_rbsp", "", 0, 0, false, {}};
    //
    // 0) BitReader r(nal_data, size);
    //    r.readBits(8);                 // 跳过 1 字节 NAL header → cursor 到 bit 8
    //                                   // (profile_idc 因此从 bit_offset=8 开始)
    //
    // 1) addField(root, "profile_idc",          r, u(8));
    //    int profile = std::stoi(root.children.back().value);   // 记下供分支判断
    //    addField(root, "constraint_set_flags", r, u(8));        // 6 flag + 2 reserved 合一
    //    addField(root, "level_idc",            r, u(8));
    //    addField(root, "seq_parameter_set_id", r, ue);
    //
    // 2) if (isHighProfile(profile)) {
    //        addField(root, "chroma_format_idc", r, ue);
    //        // 注:chroma_format_idc==3 的 separate_colour_plane_flag 属 MVP 外,不解
    //        addField(root, "bit_depth_luma_minus8",   r, ue);
    //        addField(root, "bit_depth_chroma_minus8", r, ue);
    //        addField(root, "qpprime_y_zero_transform_bypass_flag", r, u(1));
    //        // seq_scaling_matrix_present_flag:本应读 u(1);MVP 假设为 0 不解 scaling list。
    //        // 读出来若为 1,把 root.incomplete=true 并 return(见 reference doc)。
    //        addField(root, "seq_scaling_matrix_present_flag", r, u(1));
    //    }
    //
    // 3) addField(root, "log2_max_frame_num_minus4", r, ue);
    //    addField(root, "pic_order_cnt_type",        r, ue);
    //    int poc_type = std::stoi(root.children.back().value);
    //    if (poc_type == 0)
    //        addField(root, "log2_max_pic_order_cnt_lsb_minus4", r, ue);
    //    // poc_type==1 的字段属 MVP 外
    //
    // 4) addField(root, "max_num_ref_frames",                  r, ue);
    //    addField(root, "gaps_in_frame_num_value_allowed_flag", r, u(1));
    //    addField(root, "pic_width_in_mbs_minus1",        r, ue);
    //    addField(root, "pic_height_in_map_units_minus1", r, ue);
    //    addField(root, "frame_mbs_only_flag",            r, u(1));
    //    int fmo = std::stoi(root.children.back().value);
    //    if (fmo == 0)
    //        addField(root, "mb_adaptive_frame_field_flag", r, u(1));
    //    addField(root, "direct_8x8_inference_flag", r, u(1));
    //
    // 5) addField(root, "frame_cropping_flag", r, u(1));
    //    int crop = std::stoi(root.children.back().value);
    //    if (crop) {
    //        addField(root, "frame_crop_left_offset",   r, ue);
    //        addField(root, "frame_crop_right_offset",  r, ue);
    //        addField(root, "frame_crop_top_offset",    r, ue);
    //        addField(root, "frame_crop_bottom_offset", r, ue);
    //    }
    //    addField(root, "vui_parameters_present_flag", r, u(1));  // VUI 本体 MVP 不解
    //
    // 6) if (r.hasError()) root.incomplete = true;   // 宽容解析,不抛异常
    //
    // 其中 u(n) 写作 [](BitReader& br){ return br.readBits(n); };
    //       ue    写作 [](BitReader& br){ return br.readUE(); };

    if (nal_data == nullptr || size < 1) return root;
    BitReader r(nal_data, size);
    r.readBits(8);
    addField(root, "profile_idc", r, [](BitReader& br){ return br.readBits(8); });
    int profile = std::stoi(root.children.back().value);
    addField(root, "constraint_set_flags", r, [](BitReader& br){ return br.readBits(8); });
    addField(root, "level_idc", r, [](BitReader& br){ return br.readBits(8); });
    addField(root, "seq_parameter_set_id", r, [](BitReader& br){ return br.readUE(); });
    if (isHighProfile(profile)) {
        addField(root, "chroma_format_idc", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "bit_depth_luma_minus8", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "bit_depth_chroma_minus8", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "qpprime_y_zero_transform_bypass_flag", r, [](BitReader& br){ return br.readBits(1); });
        addField(root, "seq_scaling_matrix_present_flag", r, [](BitReader& br){ return br.readBits(1); });
        if (std::stoi(root.children.back().value) != 0) {
            root.incomplete = true;
            return root;
        }
    }
    addField(root, "log2_max_frame_num_minus4", r, [](BitReader& br){ return br.readUE(); });
    addField(root, "pic_order_cnt_type", r, [](BitReader& br){ return br.readUE(); });
    int poc_type = std::stoi(root.children.back().value);
    if (poc_type == 0) {
        addField(root, "log2_max_pic_order_cnt_lsb_minus4", r, [](BitReader& br){ return br.readUE(); });
    }
    addField(root, "max_num_ref_frames", r, [](BitReader& br){ return br.readUE(); });
    addField(root, "gaps_in_frame_num_value_allowed_flag", r, [](BitReader& br){ return br.readBits(1); });
    addField(root, "pic_width_in_mbs_minus1", r, [](BitReader& br){ return br.readUE(); });
    addField(root, "pic_height_in_map_units_minus1", r, [](BitReader& br){ return br.readUE(); });
    addField(root, "frame_mbs_only_flag", r, [](BitReader& br){ return br.readBits(1); });
    int fmo = std::stoi(root.children.back().value);
    if (fmo == 0) {
        addField(root, "mb_adaptive_frame_field_flag", r, [](BitReader& br){ return br.readBits(1); });
    }
    addField(root, "direct_8x8_inference_flag", r, [](BitReader& br){ return br.readBits(1); });
    addField(root, "frame_cropping_flag", r, [](BitReader& br){ return br.readBits(1); });
    int crop = std::stoi(root.children.back().value);
    if (crop) {
        addField(root, "frame_crop_left_offset", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "frame_crop_right_offset", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "frame_crop_top_offset", r, [](BitReader& br){ return br.readUE(); });
        addField(root, "frame_crop_bottom_offset", r, [](BitReader& br){ return br.readUE(); });
    }
    addField(root, "vui_parameters_present_flag", r, [](BitReader& br){ return br.readBits(1); });
    if (r.hasError()) root.incomplete = true;
    return root;
}

SyntaxNode H264SyntaxParser::parsePPS(const uint8_t* nal_data, size_t size) {
    SyntaxNode root{"pic_parameter_set_rbsp", "", 0, 0, false, {}};
    // 复用 parseSPS 里的 addField helper;lambda 写法同前。
    //
    // 0) 防御:nal_data==nullptr || size<1 → return root;
    //    BitReader r(nal_data, size);
    //    r.readBits(8);                 // 跳过 NAL header → 第一个字段从 bit 8
    //
    // 1) addField(root, "pic_parameter_set_id", r, ue);
    //    addField(root, "seq_parameter_set_id", r, ue);
    //    addField(root, "entropy_coding_mode_flag", r, u(1));
    //    addField(root, "bottom_field_pic_order_in_frame_present_flag", r, u(1));
    //
    // 2) addField(root, "num_slice_groups_minus1", r, ue);
    //    if (num_slice_groups_minus1 != 0) {     // FMO,MVP 不解
    //        root.incomplete = true; return root;
    //    }
    //
    // 3) addField(root, "num_ref_idx_l0_default_active_minus1", r, ue);
    //    addField(root, "num_ref_idx_l1_default_active_minus1", r, ue);
    //    addField(root, "weighted_pred_flag",  r, u(1));
    //    addField(root, "weighted_bipred_idc", r, u(2));        // 注意是 2 bit
    //
    // 4) 三个有符号字段(第一次用 readSE):
    //    addField(root, "pic_init_qp_minus26",     r, se);      // 可能为负,如 -3
    //    addField(root, "pic_init_qs_minus26",     r, se);
    //    addField(root, "chroma_qp_index_offset",  r, se);      // 可能为负,如 -2
    //
    // 5) addField(root, "deblocking_filter_control_present_flag", r, u(1));
    //    addField(root, "constrained_intra_pred_flag",            r, u(1));
    //    addField(root, "redundant_pic_cnt_present_flag",         r, u(1));
    //    // 到此为止。后面可能还有 PPS 扩展(more_rbsp_data),MVP 不解。
    //
    // 6) if (r.hasError()) root.incomplete = true;
    //
    // 其中 se 写作 [](BitReader& br){ return br.readSE(); };
    if (nal_data == nullptr || size < 1) return root;
    BitReader r(nal_data, size);
    r.readBits(8);
    addField(root, "pic_parameter_set_id", r, [](BitReader &r){ return r.readUE(); });
    addField(root, "seq_parameter_set_id", r, [](BitReader &r){ return r.readUE(); });
    addField(root, "entropy_coding_mode_flag", r, [](BitReader &r){ return r.readBits(1); });
    addField(root, "bottom_field_pic_order_in_frame_present_flag", r, [](BitReader &r){ return r.readBits(1); });
    addField(root, "num_slice_groups_minus1", r, [](BitReader &r){ return r.readUE(); });
    if (std::stoi(root.children.back().value) != 0) {
        root.incomplete = true;
        return root;
    }
    addField(root, "num_ref_idx_l0_default_active_minus1", r, [](BitReader &r){ return r.readUE(); });
    addField(root, "num_ref_idx_l1_default_active_minus1", r, [](BitReader &r){ return r.readUE(); });
    addField(root, "weighted_pred_flag", r, [](BitReader &r){ return r.readBits(1); });
    addField(root, "weighted_bipred_idc", r, [](BitReader &r){ return r.readBits(2); });
    addField(root, "pic_init_qp_minus26", r, [](BitReader &r){ return r.readSE(); });
    addField(root, "pic_init_qs_minus26", r, [](BitReader &r){ return r.readSE(); });
    addField(root, "chroma_qp_index_offset", r, [](BitReader &r){ return r.readSE(); });
    addField(root, "deblocking_filter_control_present_flag", r, [](BitReader &r){ return r.readBits(1); });
    addField(root, "constrained_intra_pred_flag", r, [](BitReader &r){ return r.readBits(1); });
    addField(root, "redundant_pic_cnt_present_flag", r, [](BitReader &r){ return r.readBits(1); });
    if (r.hasError()) root.incomplete = true;
    return root;
}

SyntaxNode H264SyntaxParser::parseSliceHeader(const uint8_t* nal_data, size_t size,
                                              uint8_t nal_unit_type) {
    SyntaxNode root{"slice_header", "", 0, 0, false, {}};

    // TODO(B.6b green):解析 slice header(§7.3.3),解到 pic_order_cnt_lsb 即停。
    // 复用 addField helper;另有两个 helper 可用:
    //   findIntField(tree, "name", fallback)  从已解析 SPS/PPS 树取整数字段
    //   sliceTypeLetter(st)                   slice_type 数值 → "I"/"P"/"B"...
    //
    // 0) 防御 + 跳 header:
    //      if (nal_data == nullptr || size < 1) { root.incomplete = true; return root; }
    //      BitReader r(nal_data, size);
    //      r.readBits(8);
    //
    // 1) addField(root, "first_mb_in_slice", r, ue);
    //
    //    // slice_type 要带可读后缀,不能直接用 addField(它只写数字)。手写:
    //    {
    //        size_t start = r.bitOffset();
    //        int st = static_cast<int>(r.readUE());
    //        std::string val = std::to_string(st) + " (" + sliceTypeLetter(st) + ")";
    //        root.children.push_back(SyntaxNode{
    //            "slice_type", val, start, r.bitOffset() - start, false, {}});
    //    }
    //
    //    addField(root, "pic_parameter_set_id", r, ue);
    //    int pps_id = std::stoi(root.children.back().value);
    //
    // 2) 回查上下文(没有就优雅止损):
    //      const SyntaxNode* pps = psets_ ? psets_->findPPS(pps_id) : nullptr;
    //      const SyntaxNode* sps = nullptr;
    //      if (pps) {
    //          int sps_id = findIntField(*pps, "seq_parameter_set_id", -1);
    //          sps = psets_->findSPS(sps_id);
    //      }
    //      if (sps == nullptr) { root.incomplete = true; return root; }
    //
    //      int log2_fn   = findIntField(*sps, "log2_max_frame_num_minus4", 0);
    //      int fmo_only  = findIntField(*sps, "frame_mbs_only_flag", 1);
    //      int poc_type  = findIntField(*sps, "pic_order_cnt_type", 0);
    //      int log2_poc  = findIntField(*sps, "log2_max_pic_order_cnt_lsb_minus4", 0);
    //
    // 3) frame_num 是 u(v),v = log2_fn + 4。捕获 v 的 lambda:
    //      int fn_bits = log2_fn + 4;
    //      addField(root, "frame_num", r, [fn_bits](BitReader& br){ return br.readBits(fn_bits); });
    //
    //      if (fmo_only == 0) {
    //          addField(root, "field_pic_flag", r, u(1));
    //          if (std::stoi(root.children.back().value) != 0)
    //              addField(root, "bottom_field_flag", r, u(1));
    //      }
    //
    // 4) if (nal_unit_type == 5)        // IdrPicFlag
    //        addField(root, "idr_pic_id", r, ue);
    //
    // 5) if (poc_type == 0) {
    //        int poc_bits = log2_poc + 4;
    //        addField(root, "pic_order_cnt_lsb", r, [poc_bits](BitReader& br){ return br.readBits(poc_bits); });
    //        // delta_pic_order_cnt_bottom 等属 MVP 外,不解。
    //    }
    //
    // 6) if (r.hasError()) root.incomplete = true;
    //    return root;

    (void)nal_data;
    (void)size;
    (void)nal_unit_type;
    return root;
}

}  // namespace parser
