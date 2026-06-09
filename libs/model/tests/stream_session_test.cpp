#include <gtest/gtest.h>

#include <vector>

#include "model/stream_session.h"

using model::SliceType;
using model::StreamSession;

TEST(StreamSession, OpenNonexistentFails) {
    StreamSession s;
    EXPECT_FALSE(s.open("/no/such/file.h264"));
    EXPECT_TRUE(s.frames().empty());
}

TEST(StreamSession, IdrOnlyHasFiveISlices) {
    StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/01_idr_only.h264"));
    EXPECT_EQ(s.frames().size(), 5u);
    for (const auto& f : s.frames()) {
        EXPECT_EQ(f.type, SliceType::I);
        EXPECT_FALSE(f.missing_paramset);
    }
    EXPECT_GT(s.esSize(), 0u);
}

TEST(StreamSession, IpbbHasEightFramesMixedTypes) {
    StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    ASSERT_EQ(s.frames().size(), 8u);

    const std::vector<SliceType> expectType = {
        SliceType::I, SliceType::P, SliceType::B, SliceType::B,
        SliceType::I, SliceType::P, SliceType::B, SliceType::B};
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(s.frames()[i].type, expectType[i]) << "frame " << i;
}

TEST(StreamSession, IpbbPocFollowsDisplayOrder) {
    StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    ASSERT_EQ(s.frames().size(), 8u);

    // 解码顺序 I P B B I P B B → POC 0 6 2 4 0 6 2 4
    const std::vector<int> expectPoc = {0, 6, 2, 4, 0, 6, 2, 4};
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(s.frames()[i].poc, expectPoc[i]) << "frame " << i;
}

TEST(StreamSession, FramesCarryDecodeOrderIndexAndSyntaxTree) {
    StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    ASSERT_FALSE(s.frames().empty());

    for (size_t i = 0; i < s.frames().size(); ++i) {
        EXPECT_EQ(s.frames()[i].index, static_cast<int>(i));
        // 每帧应带 slice_header 语法树
        EXPECT_EQ(s.frames()[i].syntax_tree.name, "slice_header");
        EXPECT_GT(s.frames()[i].byte_size, 0u);
    }
}

TEST(StreamSession, ProgressCallbackReachesHundred) {
    StreamSession s;
    int last = -1;
    s.setProgressCallback([&](int p) { last = p; });
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    EXPECT_EQ(last, 100);
}

TEST(StreamSession, ReopenResetsState) {
    StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    ASSERT_EQ(s.frames().size(), 8u);
    ASSERT_TRUE(s.open(FIXTURES_DIR "/01_idr_only.h264"));
    EXPECT_EQ(s.frames().size(), 5u);  // 不残留上一次的 8 帧
}
