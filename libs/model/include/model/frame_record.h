#pragma once

#include <cstddef>
#include <vector>

#include "parser/nal_unit.h"
#include "parser/syntax_node.h"

namespace model {

// =============================================================================
// SliceType — 帧的预测类型(slice_type % 5 之后的归一化值)
// =============================================================================
//
// H.264 slice_type 原始取值 0..9,其中 5..9 表示"整图所有 slice 同型"。这里用
// 归一化后的 5 类(原始值 % 5)。StreamSession 把 parser 解出的 slice_type 映射
// 到本枚举,UI 帧列表的 Type 列直接显示。
enum class SliceType { P = 0, B = 1, I = 2, SP = 3, SI = 4 };

// =============================================================================
// FrameRecord — 一帧的领域模型(POD,无行为)
// =============================================================================
//
// StreamSession 解析裸流后,每个"图像"产出一条 FrameRecord。一帧可能由多个
// slice NAL 组成(first_mb_in_slice==0 起新帧),但 MVP 的 fixture 都是一帧一
// slice。
//
// 领域层纯净:只引用 parser:: 类型与 std,不含任何 Qt / FFmpeg 类型。这样
// domain 可独立编译、独立测试(见 [[feedback-ddd-tdd]])。
//
struct FrameRecord {
    // 解码顺序(bitstream 中出现的先后,从 0 递增)。
    int index = 0;

    // 显示顺序(Picture Order Count)。MVP 只算 pic_order_cnt_type==0;
    // 其他 type 暂记 0。由 POCCalculator 计算。
    int poc = 0;

    // 帧类型(取自首个 slice 的 slice_type)。
    SliceType type = SliceType::I;

    // 本帧在整个 ES 字节流中的起始偏移与总字节数(供 Hex 定位、大小列显示)。
    size_t byte_offset_in_es = 0;
    size_t byte_size = 0;

    // 组成本帧的 NAL 单元(通常含 1 个 slice NAL;可能多个)。
    std::vector<parser::NALUnit> nals;

    // 本帧首个 slice 的语法树(slice_header)。UI 右侧语法树面板展示。
    parser::SyntaxNode syntax_tree;

    // 解析该帧时找不到引用的 SPS/PPS。UI 用它提示"上下文缺失"。
    bool missing_paramset = false;
};

}  // namespace model
