#include <gtest/gtest.h>

#include "model/poc_calculator.h"

using model::POCCalculator;

TEST(POC, IDRResetsToZero) {
    POCCalculator c(/*log2_max_pic_order_cnt_lsb=*/4);  // MaxPicOrderCntLsb = 16
    EXPECT_EQ(c.compute(/*is_idr=*/true, /*pic_order_cnt_lsb=*/0), 0);
}

TEST(POC, IncrementingLSB) {
    POCCalculator c(4);
    c.compute(true, 0);
    EXPECT_EQ(c.compute(false, 2), 2);
    EXPECT_EQ(c.compute(false, 4), 4);
    EXPECT_EQ(c.compute(false, 6), 6);
}

TEST(POC, LSBWrapAroundIncreasesMSB) {
    POCCalculator c(4);  // MaxPicOrderCntLsb = 16, half = 8
    c.compute(true, 0);
    c.compute(false, 8);
    c.compute(false, 14);
    // 下一个 lsb=2:14→2 回绕,(14-2)=12 >= 8 → msb += 16 → poc = 16 + 2 = 18
    EXPECT_EQ(c.compute(false, 2), 18);
}

TEST(POC, SecondIDRResetsState) {
    POCCalculator c(4);
    c.compute(true, 0);
    c.compute(false, 8);
    c.compute(false, 14);
    // 第二个 IDR:状态归零,poc 回到 0
    EXPECT_EQ(c.compute(true, 0), 0);
    // IDR 之后继续递增,基于归零后的状态
    EXPECT_EQ(c.compute(false, 4), 4);
}

TEST(POC, MatchesFixture02DisplayOrder) {
    // fixture 02_ipbb 实际 lsb 序列(解码顺序):I=0, P=6, B=2, B=4
    // 对应显示顺序 POC:0, 6, 2, 4(B 帧 POC 小于其后的参考 P 帧)
    POCCalculator c(4);
    EXPECT_EQ(c.compute(true, 0), 0);   // IDR I
    EXPECT_EQ(c.compute(false, 6), 6);  // P
    EXPECT_EQ(c.compute(false, 2), 2);  // B
    EXPECT_EQ(c.compute(false, 4), 4);  // B
}
