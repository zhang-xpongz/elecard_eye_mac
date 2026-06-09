#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "model/frame_record.h"
#include "parser/parameter_set_store.h"

namespace model {

// =============================================================================
// StreamSession — 一个已打开码流的聚合根(Phase C 核心)
// =============================================================================
//
// 把 Phase B 的零件串成一条流水线:读文件 → NALSplitter 切 NAL → 逐 NAL 解析
// (SPS/PPS 入 ParameterSetStore,slice 产出 FrameRecord)→ POCCalculator 算
// 显示顺序。对外只暴露"帧列表 + 参数集 + 原始 ES 字节",UI 层据此渲染。
//
// 领域纯净:只依赖 parser:: / model:: / std,**零 Qt / FFmpeg**。这一层能独立
// 跑通,就证明 domain 干净(见 [[feedback-ddd-tdd]])。
//
// MVP 范围:仅 .h264 裸流(Annex-B),用 std::ifstream 整块读入。MP4 留 Phase G,
// mmap 优化留 C.3。错误用返回值表达,不抛异常(见 [[feedback-no-cpp-exceptions]])。
//
class StreamSession {
public:
    // -------------------------------------------------------------------------
    // open — 打开并解析一个 .h264 裸流文件
    //
    // 参数:path 文件路径。
    // 返回:成功 true;文件打开失败 / 读不到字节 → false(且 frames() 为空)。
    //
    // 副作用:清空并重建内部状态(es_ / frames_ / parameterSets)。可重复调用
    //   打开不同文件。重复调用前会先 clear。
    //
    // 流程(见 stream_session.cpp 的 TODO):读文件 → split → 遍历 NAL 分派解析
    //   → 按 first_mb_in_slice==0 切帧 → 算 POC。
    bool open(const std::string& path);

    // 解析得到的帧列表(解码顺序)。open 成功后有效。
    const std::vector<FrameRecord>& frames() const { return frames_; }

    // 解析过程中收集的 SPS/PPS 仓储(解码器随机访问、调试用)。
    const parser::ParameterSetStore& parameterSets() const { return psets_; }

    // 原始 ES 字节(供 Hex viewer / 解码器直接读)。open 成功后有效,
    // 生命周期同 StreamSession。
    const uint8_t* esData() const { return es_.data(); }
    size_t esSize() const { return es_.size(); }

    // -------------------------------------------------------------------------
    // setProgressCallback — 设置解析进度回调(0..100)
    //
    // open 在遍历 NAL 时按已处理比例回调。可用于 UI 进度条。未设置则不回调。
    // 回调在 open 的调用线程同步触发(Phase D 会把 open 放到 worker 线程)。
    using ProgressCb = std::function<void(int percent)>;
    void setProgressCallback(ProgressCb cb) { progress_ = std::move(cb); }

private:
    std::vector<uint8_t> es_;
    std::vector<FrameRecord> frames_;
    parser::ParameterSetStore psets_;
    ProgressCb progress_;
};

}  // namespace model
