#include <gtest/gtest.h>

#include "parser/nal_splitter.h"

using parser::NALSplitter;

TEST(NALSplitter, ThreeByteStartCode) {
    // 起始码 00 00 01 + NAL header 0x67 (forbidden=0, nal_ref_idc=3, type=7 SPS)
    // + payload 0x42 0xC0 0x1F
    const uint8_t buf[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].byte_offset, 0u);
    EXPECT_EQ(nals[0].payload_offset, 3u);
    EXPECT_EQ(nals[0].size, 4u);
    EXPECT_EQ(nals[0].nal_unit_type, 0x07u);  // SPS
}

TEST(NALSplitter, FourByteStartCode) {
    // 起始码 00 00 00 01 + NAL header 0x67 + payload
    const uint8_t buf[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].byte_offset, 0u);   // 含 leading 00
    EXPECT_EQ(nals[0].payload_offset, 4u);
    EXPECT_EQ(nals[0].size, 4u);
    EXPECT_EQ(nals[0].nal_unit_type, 0x07u);
}

TEST(NALSplitter, MultipleNALs) {
    const uint8_t buf[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F,   // NAL 1: SPS  (4-byte sc)
        0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x06, 0xE2,   // NAL 2: PPS  (4-byte sc)
        0x00, 0x00, 0x01,       0x65, 0x88, 0x80, 0x10    // NAL 3: IDR  (3-byte sc)
    };
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 3u);

    EXPECT_EQ(nals[0].byte_offset,    0u);
    EXPECT_EQ(nals[0].payload_offset, 4u);
    EXPECT_EQ(nals[0].size,           4u);
    EXPECT_EQ(nals[0].nal_unit_type,  7u);   // SPS

    EXPECT_EQ(nals[1].byte_offset,    8u);
    EXPECT_EQ(nals[1].payload_offset, 12u);
    EXPECT_EQ(nals[1].size,           4u);
    EXPECT_EQ(nals[1].nal_unit_type,  8u);   // PPS

    EXPECT_EQ(nals[2].byte_offset,    16u);
    EXPECT_EQ(nals[2].payload_offset, 19u);
    EXPECT_EQ(nals[2].size,           4u);
    EXPECT_EQ(nals[2].nal_unit_type,  5u);   // IDR slice
}

TEST(NALSplitter, EmulationPreventionDoesNotSplit) {
    // payload 里出现 00 00 03(EP3 escape 字节序列),不是起始码,不应被切
    const uint8_t buf[] = {
        0x00, 0x00, 0x00, 0x01,                        // start code
        0x67, 0x00, 0x00, 0x03, 0x01, 0xFF             // payload(含 EP3)
    };
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].size, 6u);   // 整段 payload 不被误切
}

TEST(NALSplitter, TruncatedTrailingBytes) {
    // 文件末尾起始码后只有 1 字节
    const uint8_t buf[] = {0x00, 0x00, 0x00, 0x01, 0x67};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].size, 1u);
    EXPECT_EQ(nals[0].nal_unit_type, 7u);
}

TEST(NALSplitter, EmptyBuffer) {
    NALSplitter s;
    EXPECT_TRUE(s.split(nullptr, 0).empty());
    const uint8_t empty[] = {0x00};
    EXPECT_TRUE(s.split(empty, 0).empty());
    EXPECT_TRUE(s.split(empty, 1).empty());  // 太短装不下起始码
}
