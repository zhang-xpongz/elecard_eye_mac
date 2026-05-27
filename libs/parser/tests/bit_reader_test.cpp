#include <gtest/gtest.h>

#include "parser/bit_reader.h"

using parser::BitReader;

TEST(BitReader, ReadOneBitAtATime) {
    // 0xB4 = 1011 0100
    const uint8_t data[] = {0xB4};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_FALSE(r.hasError());
}

TEST(BitReader, ReadAcrossByteBoundary) {
    // 0xAB 0xCD = 1010 1011 1100 1101
    const uint8_t data[] = {0xAB, 0xCD};
    BitReader r(data, 2);
    EXPECT_EQ(r.readBits(4), 0xAu);   // 1010
    EXPECT_EQ(r.readBits(8), 0xBCu);  // 1011 1100 (跨字节)
    EXPECT_EQ(r.readBits(4), 0xDu);   // 1101
    EXPECT_FALSE(r.hasError());
}

TEST(BitReader, BitOffsetTracking) {
    const uint8_t data[] = {0xFF, 0xFF};
    BitReader r(data, 2);
    EXPECT_EQ(r.bitOffset(), 0u);
    r.readBits(3);
    EXPECT_EQ(r.bitOffset(), 3u);
    r.readBits(7);
    EXPECT_EQ(r.bitOffset(), 10u);
    r.readBits(6);
    EXPECT_EQ(r.bitOffset(), 16u);
}

TEST(BitReader, UnsignedExpGolomb) {
    // 序列: ue={1,2,3,4,7}
    //   1 = 010
    //   2 = 011
    //   3 = 00100
    //   4 = 00101
    //   7 = 0001000
    // 拼接: 010 011 00100 00101 0001000 = 0100 1100 1000 0101 0001 000 (23 bits)
    // 补 0 凑到 24 bits: 0100 1100 1000 0101 0001 0000
    //                 = 0x4C 0x85 0x10
    const uint8_t data[] = {0x4C, 0x85, 0x10};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readUE(), 1u);
    EXPECT_EQ(r.readUE(), 2u);
    EXPECT_EQ(r.readUE(), 3u);
    EXPECT_EQ(r.readUE(), 4u);
    EXPECT_EQ(r.readUE(), 7u);
    EXPECT_FALSE(r.hasError());
}

TEST(BitReader, SignedExpGolomb) {
    // se 映射: ue=0→0, ue=1→+1, ue=2→-1, ue=3→+2
    // 序列: se={0, +1, -1, +2} → ue 序列: 0, 1, 2, 3
    //   0 = 1
    //   1 = 010
    //   2 = 011
    //   3 = 00100
    // 拼接: 1 010 011 00100 = 1010 0110 0100 (12 bits)
    // 补 0 凑到 16 bits: 1010 0110 0100 0000 = 0xA6 0x40
    const uint8_t data[] = {0xA6, 0x40};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readSE(), 0);
    EXPECT_EQ(r.readSE(), 1);
    EXPECT_EQ(r.readSE(), -1);
    EXPECT_EQ(r.readSE(), 2);
    EXPECT_FALSE(r.hasError());
}

TEST(BitReader, OutOfRangeSetsErrorFlag) {
    const uint8_t data[] = {0xFF};
    BitReader r(data, 1);
    r.readBits(8);
    EXPECT_FALSE(r.hasError());
    r.readBits(1);  // 越界
    EXPECT_TRUE(r.hasError());
}
