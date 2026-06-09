#include <gtest/gtest.h>

#include "parser/ep3_strip.h"
#include "parser/h264_syntax_parser.h"

using parser::H264SyntaxParser;
using parser::SyntaxNode;

// 深度优先查第一个 name 匹配的节点;找不到返回 nullptr。
static const SyntaxNode* find(const SyntaxNode& n, const std::string& name) {
    if (n.name == name) return &n;
    for (const auto& c : n.children) {
        if (const auto* hit = find(c, name)) return hit;
    }
    return nullptr;
}

// fixture 01_idr_only.h264 的真实 SPS NAL(含 0x67 header,EP3 未剥)。
// x264 Baseline, 128x128。字段值由独立 Python 解析器核对。
static const uint8_t kSpsBaseline[] = {
    0x67, 0x42, 0xC0, 0x0A, 0xDC, 0x20, 0x46, 0xC0, 0x44, 0x00, 0x00, 0x03,
    0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x28, 0x3C, 0x48, 0x9E, 0x00};

// fixture 03_with_b_refs.h264 的真实 SPS NAL(含 header,EP3 未剥)。
// x264 High, 128x128。
static const uint8_t kSpsHigh[] = {
    0x67, 0x64, 0x00, 0x0A, 0xAC, 0xD9, 0x42, 0x04, 0x6C, 0x04, 0x40, 0x00,
    0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x08, 0x03, 0xC4, 0x89, 0x65, 0x80,
    0x00};

static SyntaxNode parseSps(const uint8_t* ebsp, size_t n) {
    auto rbsp = parser::stripEmulationPrevention(ebsp, n);
    H264SyntaxParser p;
    return p.parseNAL(rbsp.data(), rbsp.size(), /*nal_unit_type=*/7);
}

TEST(H264SPS, BaselineProfile128x128) {
    auto tree = parseSps(kSpsBaseline, sizeof(kSpsBaseline));

    ASSERT_NE(find(tree, "profile_idc"), nullptr);
    EXPECT_EQ(find(tree, "profile_idc")->value, "66");
    EXPECT_EQ(find(tree, "level_idc")->value, "10");
    EXPECT_EQ(find(tree, "seq_parameter_set_id")->value, "0");
    EXPECT_EQ(find(tree, "pic_order_cnt_type")->value, "2");
    EXPECT_EQ(find(tree, "pic_width_in_mbs_minus1")->value, "7");        // 128/16-1
    EXPECT_EQ(find(tree, "pic_height_in_map_units_minus1")->value, "7");
    EXPECT_EQ(find(tree, "frame_mbs_only_flag")->value, "1");
    EXPECT_EQ(find(tree, "vui_parameters_present_flag")->value, "1");

    // Baseline 不是 High profile,这些字段不应出现
    EXPECT_EQ(find(tree, "chroma_format_idc"), nullptr);
    // pic_order_cnt_type==2,不应读 lsb 字段
    EXPECT_EQ(find(tree, "log2_max_pic_order_cnt_lsb_minus4"), nullptr);

    EXPECT_FALSE(tree.incomplete);
}

TEST(H264SPS, HighProfileHasChromaAndPocLsb) {
    auto tree = parseSps(kSpsHigh, sizeof(kSpsHigh));

    EXPECT_EQ(find(tree, "profile_idc")->value, "100");
    EXPECT_EQ(find(tree, "level_idc")->value, "10");

    // High profile 专有字段
    ASSERT_NE(find(tree, "chroma_format_idc"), nullptr);
    EXPECT_EQ(find(tree, "chroma_format_idc")->value, "1");
    EXPECT_EQ(find(tree, "bit_depth_luma_minus8")->value, "0");
    EXPECT_EQ(find(tree, "bit_depth_chroma_minus8")->value, "0");

    // pic_order_cnt_type==0 → 应读出 lsb 字段
    EXPECT_EQ(find(tree, "pic_order_cnt_type")->value, "0");
    ASSERT_NE(find(tree, "log2_max_pic_order_cnt_lsb_minus4"), nullptr);
    EXPECT_EQ(find(tree, "log2_max_pic_order_cnt_lsb_minus4")->value, "2");

    EXPECT_EQ(find(tree, "max_num_ref_frames")->value, "4");
    EXPECT_FALSE(tree.incomplete);
}

TEST(H264SPS, FieldsAreContiguousFromBit8) {
    auto tree = parseSps(kSpsBaseline, sizeof(kSpsBaseline));

    ASSERT_FALSE(tree.children.empty());
    // 第一个字段 profile_idc 紧跟 1 字节 NAL header → bit 8 起
    EXPECT_EQ(tree.children.front().name, "profile_idc");
    EXPECT_EQ(tree.children.front().bit_offset, 8u);

    // 扁平子节点应首尾相接,无空洞、无重叠
    for (size_t i = 1; i < tree.children.size(); ++i) {
        const auto& prev = tree.children[i - 1];
        const auto& cur = tree.children[i];
        EXPECT_EQ(cur.bit_offset, prev.bit_offset + prev.bit_length)
            << "gap/overlap before child '" << cur.name << "'";
        EXPECT_GT(cur.bit_length, 0u) << "zero-length field '" << cur.name << "'";
    }
}
