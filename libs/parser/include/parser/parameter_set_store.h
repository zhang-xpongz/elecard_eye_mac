#pragma once

#include <unordered_map>

#include "parser/syntax_node.h"

namespace parser {

// =============================================================================
// ParameterSetStore — 已解析 SPS / PPS 的按 id 仓储
// =============================================================================
//
// 存放 H264SyntaxParser 解出的 SPS / PPS 语法树,供后续按 id 回查。Slice header
// 解析(B.6b)需要它:slice 里的 frame_num / pic_order_cnt_lsb 位宽取自所引用
// SPS 的字段,slice 通过 pic_parameter_set_id → PPS → seq_parameter_set_id →
// SPS 这条链找到上下文。
//
// 命名空间归属:放在 parser:: 而非 model::。它存的是 parser::SyntaxNode,概念上
// 是 parser 层的"已解析头部仓储";这样 parser 子层自洽可测,不依赖 model。
// Model / decoder 层后续直接复用本类型(见 plan §B.6 的 DDD 决策)。
//
// 不抛异常:查不到返回 nullptr,由调用方判断(slice 解析时标 missing_paramset)。
//
class ParameterSetStore {
public:
    // -------------------------------------------------------------------------
    // addSPS / addPPS — 存入一棵已解析的参数集树
    //
    // id    seq_parameter_set_id / pic_parameter_set_id(合法范围 SPS 0..31,
    //       PPS 0..255;本类不校验范围,按传入 id 存)。
    // tree  解析得到的 SyntaxNode(按值传入,内部移动保存)。
    //
    // 语义:同 id 重复 add 覆盖旧值(参数集可在流中途被重新定义,后者生效)。
    void addSPS(int id, SyntaxNode tree);
    void addPPS(int id, SyntaxNode tree);

    // -------------------------------------------------------------------------
    // findSPS / findPPS — 按 id 查参数集
    //
    // 返回:命中则返回指向内部存储节点的指针(仅在下次修改本 store 前有效);
    //       未命中返回 nullptr。
    const SyntaxNode* findSPS(int id) const;
    const SyntaxNode* findPPS(int id) const;

    // -------------------------------------------------------------------------
    // clear — 清空所有 SPS 与 PPS(打开新文件时调用)
    void clear();

private:
    std::unordered_map<int, SyntaxNode> sps_;
    std::unordered_map<int, SyntaxNode> pps_;
};

}  // namespace parser
