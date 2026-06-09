#include <gtest/gtest.h>

#include "parser/ep3_strip.h"
#include "parser/h264_syntax_parser.h"
#include "parser/parameter_set_store.h"

using parser::H264SyntaxParser;
using parser::ParameterSetStore;
using parser::SyntaxNode;

static const SyntaxNode* find(const SyntaxNode& n, const std::string& name) {
    if (n.name == name) return &n;
    for (const auto& c : n.children) {
        if (const auto* hit = find(c, name)) return hit;
    }
    return nullptr;
}

static int fieldInt(const SyntaxNode& n, const std::string& name) {
    const auto* h = find(n, name);
    return h ? std::stoi(h->value) : -1;
}

static SyntaxNode parseNal(H264SyntaxParser& p, const uint8_t* ebsp, size_t n,
                           uint8_t nut) {
    auto rbsp = parser::stripEmulationPrevention(ebsp, n);
    return p.parseNAL(rbsp.data(), rbsp.size(), nut);
}

// 把一段 SPS / PPS 字节解析后按其 id 注册进 store。
static void registerSps(ParameterSetStore& s, H264SyntaxParser& p,
                        const uint8_t* b, size_t n) {
    auto t = parseNal(p, b, n, 7);
    s.addSPS(fieldInt(t, "seq_parameter_set_id"), t);
}
static void registerPps(ParameterSetStore& s, H264SyntaxParser& p,
                        const uint8_t* b, size_t n) {
    auto t = parseNal(p, b, n, 8);
    s.addPPS(fieldInt(t, "pic_parameter_set_id"), t);
}

// ---- fixture 02_ipbb (Main, POC type 0,有 I/P/B) ----
static const uint8_t kSps02[] = {
    0x67, 0x4D, 0x40, 0x0A, 0xEC, 0xA1, 0x02, 0x36, 0x02, 0x20, 0x00, 0x00,
    0x03, 0x00, 0x20, 0x00, 0x00, 0x03, 0x02, 0x01, 0xE2, 0x44, 0xB2, 0xC0,
    0x00};
static const uint8_t kPps02[] = {0x68, 0xEB, 0xE3, 0xCB, 0x20};
static const uint8_t kIdr02[] = {0x65, 0x88, 0x84, 0x00, 0x10, 0xFF, 0xFE, 0xF7};
static const uint8_t kP02[]   = {0x41, 0x9A, 0x23, 0x6C, 0x43, 0xBF, 0xFE, 0xA9};
static const uint8_t kB02[]   = {0x41, 0x9E, 0x41, 0x78, 0x87, 0x7F, 0x00, 0x8D};

// ---- fixture 01_idr_only (Baseline, POC type 2,无 pic_order_cnt_lsb) ----
static const uint8_t kSps01[] = {
    0x67, 0x42, 0xC0, 0x0A, 0xDC, 0x20, 0x46, 0xC0, 0x44, 0x00, 0x00, 0x03,
    0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x28, 0x3C, 0x48, 0x9E, 0x00};
static const uint8_t kPps01[] = {0x68, 0xCE, 0x0F, 0x2C, 0x80};
static const uint8_t kIdr01[] = {0x65, 0x88, 0x84, 0x04, 0xBC, 0x98, 0xA0, 0x00};

TEST(H264SliceHeader, IdrISlicePocType0) {
    ParameterSetStore s;
    H264SyntaxParser p;
    registerSps(s, p, kSps02, sizeof(kSps02));
    registerPps(s, p, kPps02, sizeof(kPps02));
    p.setParameterSets(&s);

    auto t = parseNal(p, kIdr02, sizeof(kIdr02), /*IDR=*/5);

    EXPECT_EQ(find(t, "first_mb_in_slice")->value, "0");
    EXPECT_EQ(find(t, "slice_type")->value, "7 (I)");
    EXPECT_EQ(find(t, "pic_parameter_set_id")->value, "0");
    EXPECT_EQ(find(t, "frame_num")->value, "0");
    EXPECT_EQ(find(t, "idr_pic_id")->value, "0");
    EXPECT_EQ(find(t, "pic_order_cnt_lsb")->value, "0");
    EXPECT_FALSE(t.incomplete);
}

TEST(H264SliceHeader, PSliceHasNoIdrPicId) {
    ParameterSetStore s;
    H264SyntaxParser p;
    registerSps(s, p, kSps02, sizeof(kSps02));
    registerPps(s, p, kPps02, sizeof(kPps02));
    p.setParameterSets(&s);

    auto t = parseNal(p, kP02, sizeof(kP02), /*non-IDR=*/1);

    EXPECT_EQ(find(t, "slice_type")->value, "5 (P)");
    EXPECT_EQ(find(t, "frame_num")->value, "1");
    EXPECT_EQ(find(t, "pic_order_cnt_lsb")->value, "6");
    // 非 IDR slice 不应有 idr_pic_id
    EXPECT_EQ(find(t, "idr_pic_id"), nullptr);
    EXPECT_FALSE(t.incomplete);
}

TEST(H264SliceHeader, BSlice) {
    ParameterSetStore s;
    H264SyntaxParser p;
    registerSps(s, p, kSps02, sizeof(kSps02));
    registerPps(s, p, kPps02, sizeof(kPps02));
    p.setParameterSets(&s);

    auto t = parseNal(p, kB02, sizeof(kB02), 1);

    EXPECT_EQ(find(t, "slice_type")->value, "6 (B)");
    EXPECT_EQ(find(t, "frame_num")->value, "2");
    EXPECT_EQ(find(t, "pic_order_cnt_lsb")->value, "2");
    EXPECT_FALSE(t.incomplete);
}

TEST(H264SliceHeader, PocType2HasNoLsb) {
    ParameterSetStore s;
    H264SyntaxParser p;
    registerSps(s, p, kSps01, sizeof(kSps01));
    registerPps(s, p, kPps01, sizeof(kPps01));
    p.setParameterSets(&s);

    auto t = parseNal(p, kIdr01, sizeof(kIdr01), 5);

    EXPECT_EQ(find(t, "slice_type")->value, "7 (I)");
    EXPECT_EQ(find(t, "frame_num")->value, "0");
    EXPECT_EQ(find(t, "idr_pic_id")->value, "0");
    // SPS pic_order_cnt_type==2 → 不读 pic_order_cnt_lsb
    EXPECT_EQ(find(t, "pic_order_cnt_lsb"), nullptr);
    EXPECT_FALSE(t.incomplete);
}

TEST(H264SliceHeader, MissingParameterSetMarksIncomplete) {
    H264SyntaxParser p;
    // 没有 setParameterSets,store 为空 → 无法取 frame_num 位宽
    auto t = parseNal(p, kIdr02, sizeof(kIdr02), 5);

    // pic_parameter_set_id 之前的字段仍解出
    EXPECT_EQ(find(t, "slice_type")->value, "7 (I)");
    EXPECT_EQ(find(t, "pic_parameter_set_id")->value, "0");
    // 之后无法继续
    EXPECT_TRUE(t.incomplete);
    EXPECT_EQ(find(t, "frame_num"), nullptr);
}

TEST(H264SliceHeader, FieldsAreContiguousFromBit8) {
    ParameterSetStore s;
    H264SyntaxParser p;
    registerSps(s, p, kSps02, sizeof(kSps02));
    registerPps(s, p, kPps02, sizeof(kPps02));
    p.setParameterSets(&s);

    auto t = parseNal(p, kIdr02, sizeof(kIdr02), 5);

    ASSERT_FALSE(t.children.empty());
    EXPECT_EQ(t.children.front().name, "first_mb_in_slice");
    EXPECT_EQ(t.children.front().bit_offset, 8u);
    for (size_t i = 1; i < t.children.size(); ++i) {
        const auto& prev = t.children[i - 1];
        const auto& cur = t.children[i];
        EXPECT_EQ(cur.bit_offset, prev.bit_offset + prev.bit_length)
            << "gap/overlap before child '" << cur.name << "'";
    }
}
