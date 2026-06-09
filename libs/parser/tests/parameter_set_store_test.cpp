#include <gtest/gtest.h>

#include "parser/parameter_set_store.h"

using parser::ParameterSetStore;
using parser::SyntaxNode;

static SyntaxNode makeNode(const std::string& name, const std::string& value) {
    return SyntaxNode{name, value, 0, 0, false, {}};
}

TEST(ParameterSetStore, FindMissingReturnsNull) {
    ParameterSetStore s;
    EXPECT_EQ(s.findSPS(0), nullptr);
    EXPECT_EQ(s.findPPS(0), nullptr);
}

TEST(ParameterSetStore, AddAndFindSPS) {
    ParameterSetStore s;
    s.addSPS(0, makeNode("seq_parameter_set_rbsp", "sps0"));
    s.addSPS(3, makeNode("seq_parameter_set_rbsp", "sps3"));

    ASSERT_NE(s.findSPS(0), nullptr);
    EXPECT_EQ(s.findSPS(0)->value, "sps0");
    ASSERT_NE(s.findSPS(3), nullptr);
    EXPECT_EQ(s.findSPS(3)->value, "sps3");
    EXPECT_EQ(s.findSPS(1), nullptr);
}

TEST(ParameterSetStore, AddAndFindPPS) {
    ParameterSetStore s;
    s.addPPS(2, makeNode("pic_parameter_set_rbsp", "pps2"));

    ASSERT_NE(s.findPPS(2), nullptr);
    EXPECT_EQ(s.findPPS(2)->value, "pps2");
    // SPS 与 PPS 命名空间相互独立:同 id 不串
    EXPECT_EQ(s.findSPS(2), nullptr);
}

TEST(ParameterSetStore, ReaddSameIdOverwrites) {
    ParameterSetStore s;
    s.addSPS(1, makeNode("seq_parameter_set_rbsp", "old"));
    s.addSPS(1, makeNode("seq_parameter_set_rbsp", "new"));

    ASSERT_NE(s.findSPS(1), nullptr);
    EXPECT_EQ(s.findSPS(1)->value, "new");
}

TEST(ParameterSetStore, ClearEmptiesBoth) {
    ParameterSetStore s;
    s.addSPS(0, makeNode("seq_parameter_set_rbsp", "sps0"));
    s.addPPS(0, makeNode("pic_parameter_set_rbsp", "pps0"));

    s.clear();

    EXPECT_EQ(s.findSPS(0), nullptr);
    EXPECT_EQ(s.findPPS(0), nullptr);
}
