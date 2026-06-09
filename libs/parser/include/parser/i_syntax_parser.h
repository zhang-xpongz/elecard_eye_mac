#pragma once

#include <cstddef>
#include <cstdint>

#include "parser/syntax_node.h"

namespace parser {

// =============================================================================
// ISyntaxParser — codec 无关的语法解析器接口
// =============================================================================
//
// 把"已经按 NAL 切好、且 EP3 已剥离的一段 RBSP"解析成一棵 SyntaxNode 树。
// 不同 codec(H.264 / H.265 / AV1 ...)各实现一个子类,UI 与上层只依赖本接口,
// 加 codec 不改调用方。
//
// 错误处理:不抛异常。遇到越界 / 非法字段时,把对应节点 incomplete 置 true 并
// 停止继续往下解(宽容解析)。调用方通过节点的 incomplete 标志判断是否截断。
//
class ISyntaxParser {
public:
    virtual ~ISyntaxParser() = default;

    // -------------------------------------------------------------------------
    // parseNAL — 解析单个 NAL 的 RBSP,产出语法树
    //
    // 输入约定:
    //   nal_data        指向**单个 NAL 单元**的字节,**含 1 字节 NAL header**
    //                   (forbidden_zero_bit + nal_ref_idc + nal_unit_type),
    //                   且 EP3(emulation_prevention_three_byte)已剥离。
    //                   典型来自:NALSplitter 切边界 → stripEmulationPrevention。
    //   size            nal_data 的字节数。
    //   nal_unit_type   NAL 类型(= header 低 5 bit)。由调用方传入而非重新解析,
    //                   方便分派:7=SPS,8=PPS,1/5=slice ...。
    //
    // 输出:
    //   语法树根节点。子节点的 bit_offset 以 nal_data 起点为基准(NAL 内偏移),
    //   第一个字段位于 bit 8(跳过 1 字节 NAL header)。把 NAL 在整个 ES 中的
    //   基准偏移加到这些值上,即得全局偏移(由 StreamSession 负责)。
    //   无法识别的 nal_unit_type 返回空根节点(name 为空、无子节点)。
    virtual SyntaxNode parseNAL(const uint8_t* nal_data, size_t size,
                                uint8_t nal_unit_type) = 0;
};

}  // namespace parser
