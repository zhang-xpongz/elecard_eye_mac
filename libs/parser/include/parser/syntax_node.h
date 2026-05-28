#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace parser {

// =============================================================================
// SyntaxNode — 通用语法树节点(纯 POD,无行为)
// =============================================================================
//
// Parser 把比特流解析成一棵 SyntaxNode 树。所有 H.264 语法字段(profile_idc、
// level_idc、slice_type ...)都用同一个通用结构表达,而不是为每个字段做专门
// struct。
//
// 设计动机:
//   - UI 渲染统一:SyntaxTreePanel 只认 name/value/children,不需要知道具体
//     字段类型
//   - 扩展性:加新 codec(H.265/AV1)时不用改 UI,parser 多吐 SyntaxNode 即可
//   - Hex 联动:bit_offset/bit_length 记录该字段在 ES 中的精确位置,点节点就能
//     在 Hex viewer 高亮对应字节
//
// 代价:程序化访问字段要按 name 查找(见各 parser 测试里的 find() helper),
// 而非直接成员访问。MVP 阶段可接受。
//
struct SyntaxNode {
    // 字段名,用 H.264 规范原始命名(如 "seq_parameter_set_id")。
    // 非叶子节点用结构名(如 "seq_parameter_set_rbsp")。
    std::string name;

    // 渲染后的值字符串(如 "66" / "31" / "7 (I slice)")。
    // 非叶子节点通常为空。值的格式化由 parser 决定,UI 直接显示。
    std::string value;

    // 该字段在**整个 ES 字节流**中的起始 bit 偏移(用于 Hex viewer 高亮)。
    // 注意:是全局偏移,不是 NAL 内偏移 —— parser 负责把 NAL 基址加上去。
    // 非叶子节点可设为其第一个子节点的 bit_offset。
    size_t bit_offset = 0;

    // 该字段占用的 bit 数。叶子节点 = 字段实际 bit 长度;
    // 非叶子节点可设为所有子节点 bit_length 之和(或 0,表示不参与高亮)。
    size_t bit_length = 0;

    // 截断标记。解析中途遇到 buffer 越界 / 非法值时置 true,UI 用黄色提示。
    // 体现项目"宽容解析、错误用数据形态表达、不抛异常"的原则。
    bool incomplete = false;

    // 子节点(递归构成树)。叶子节点为空。
    std::vector<SyntaxNode> children;
};

}  // namespace parser
