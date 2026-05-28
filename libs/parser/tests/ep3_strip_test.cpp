#include <gtest/gtest.h>

#include "parser/ep3_strip.h"

using parser::stripEmulationPrevention;

TEST(EP3Strip, NoEscapesUnchanged) {
    const std::vector<uint8_t> in = {0x67, 0x42, 0xC0, 0x1F};
    auto out = stripEmulationPrevention(in.data(), in.size());
    EXPECT_EQ(out, in);
}

TEST(EP3Strip, SingleEscapeRemoved) {
    // 00 00 03 01 → 00 00 01
    const std::vector<uint8_t> in  = {0x67, 0x00, 0x00, 0x03, 0x01};
    const std::vector<uint8_t> exp = {0x67, 0x00, 0x00, 0x01};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}

TEST(EP3Strip, MultipleEscapes) {
    // 两处 EP3:
    //   00 00 03 00 → 00 00 00
    //   00 00 03 01 → 00 00 01
    const std::vector<uint8_t> in  = {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x01};
    const std::vector<uint8_t> exp = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}

TEST(EP3Strip, EscapeAtEnd) {
    // 末尾的 00 00 03(03 后无字节)也剥成 00 00
    const std::vector<uint8_t> in  = {0xFF, 0x00, 0x00, 0x03};
    const std::vector<uint8_t> exp = {0xFF, 0x00, 0x00};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}

TEST(EP3Strip, EmptyInput) {
    EXPECT_TRUE(stripEmulationPrevention(nullptr, 0).empty());
}
