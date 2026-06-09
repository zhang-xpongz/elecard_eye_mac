#include <gtest/gtest.h>

#include "parser/ep3_strip.h"
#include "parser/h264_syntax_parser.h"

using parser::H264SyntaxParser;
using parser::SyntaxNode;

static const SyntaxNode* find(const SyntaxNode& n, const std::string& name) {
    if (n.name == name) return &n;
    for (const auto& c : n.children) {
        if (const auto* hit = find(c, name)) return hit;
    }
    return nullptr;
}

// fixture 01_idr_only.h264 的真实 PPS NAL(含 0x68 header,EP3 未剥)。
// x264 Baseline:CAVLC,负 se 字段。值由独立 Python 解析器核对。
static const uint8_t kPpsBaseline[] = {0x68, 0xCE, 0x0F, 0x2C, 0x80};

// fixture 03_with_b_refs.h264 的真实 PPS NAL(含 header,EP3 未剥)。
// x264 High:CABAC,num_ref_idx_l0=3,weighted_bipred_idc=2。
static const uint8_t kPpsHigh[] = {0x68, 0xE9, 0x38, 0xF2, 0xC8, 0xB0};

static SyntaxNode parsePps(const uint8_t* ebsp, size_t n) {
    auto rbsp = parser::stripEmulationPrevention(ebsp, n);
    H264SyntaxParser p;
    return p.parseNAL(rbsp.data(), rbsp.size(), /*nal_unit_type=*/8);
}

TEST(H264PPS, BaselineCavlcNegativeQp) {
    auto tree = parsePps(kPpsBaseline, sizeof(kPpsBaseline));

    ASSERT_NE(find(tree, "pic_parameter_set_id"), nullptr);
    EXPECT_EQ(find(tree, "pic_parameter_set_id")->value, "0");
    EXPECT_EQ(find(tree, "seq_parameter_set_id")->value, "0");
    EXPECT_EQ(find(tree, "entropy_coding_mode_flag")->value, "0");  // CAVLC
    EXPECT_EQ(find(tree, "weighted_pred_flag")->value, "0");
    EXPECT_EQ(find(tree, "weighted_bipred_idc")->value, "0");
    // 有符号字段(se),负值
    EXPECT_EQ(find(tree, "pic_init_qp_minus26")->value, "-3");
    EXPECT_EQ(find(tree, "chroma_qp_index_offset")->value, "-2");
    EXPECT_EQ(find(tree, "deblocking_filter_control_present_flag")->value, "1");
    EXPECT_EQ(find(tree, "redundant_pic_cnt_present_flag")->value, "0");

    EXPECT_FALSE(tree.incomplete);
}

TEST(H264PPS, HighCabacWithRefsAndWeights) {
    auto tree = parsePps(kPpsHigh, sizeof(kPpsHigh));

    EXPECT_EQ(find(tree, "entropy_coding_mode_flag")->value, "1");  // CABAC
    EXPECT_EQ(find(tree, "num_ref_idx_l0_default_active_minus1")->value, "3");
    EXPECT_EQ(find(tree, "num_ref_idx_l1_default_active_minus1")->value, "0");
    EXPECT_EQ(find(tree, "weighted_pred_flag")->value, "1");
    EXPECT_EQ(find(tree, "weighted_bipred_idc")->value, "2");
    EXPECT_EQ(find(tree, "pic_init_qp_minus26")->value, "-3");
    EXPECT_EQ(find(tree, "chroma_qp_index_offset")->value, "-2");

    EXPECT_FALSE(tree.incomplete);
}

TEST(H264PPS, FieldsAreContiguousFromBit8) {
    auto tree = parsePps(kPpsBaseline, sizeof(kPpsBaseline));

    ASSERT_FALSE(tree.children.empty());
    EXPECT_EQ(tree.children.front().name, "pic_parameter_set_id");
    EXPECT_EQ(tree.children.front().bit_offset, 8u);

    for (size_t i = 1; i < tree.children.size(); ++i) {
        const auto& prev = tree.children[i - 1];
        const auto& cur = tree.children[i];
        EXPECT_EQ(cur.bit_offset, prev.bit_offset + prev.bit_length)
            << "gap/overlap before child '" << cur.name << "'";
        EXPECT_GT(cur.bit_length, 0u) << "zero-length field '" << cur.name << "'";
    }
}
