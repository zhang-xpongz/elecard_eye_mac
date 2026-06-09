#include "parser/h264_syntax_parser.h"

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

}  // namespace

SyntaxNode H264SyntaxParser::parseNAL(const uint8_t* nal_data, size_t size,
                                      uint8_t nal_unit_type) {
    if (nal_unit_type == 7) return parseSPS(nal_data, size);
    // PPS(8)/SliceHeader(1,5) 留给 B.5 / B.6
    return SyntaxNode{};
}

SyntaxNode H264SyntaxParser::parseSPS(const uint8_t* nal_data, size_t size) {
    SyntaxNode root{"seq_parameter_set_rbsp", "", 0, 0, false, {}};

    // TODO(B.4 green):用 BitReader 顺序解析 SPS 字段。
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

}  // namespace parser
