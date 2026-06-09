#include "model/stream_session.h"

#include <climits>
#include <fstream>

#include "model/poc_calculator.h"
#include "parser/ep3_strip.h"
#include "parser/h264_syntax_parser.h"
#include "parser/nal_splitter.h"

namespace model {

namespace {

// 深度优先在语法树里取整数字段。value 形如 "7 (I)" 时 std::stoi 只读前导数字 → 7。
// 缺省返回 fallback。
int findInt(const parser::SyntaxNode& n, const char* name, int fallback) {
    if (n.name == name) return std::stoi(n.value);
    for (const auto& c : n.children) {
        const int v = findInt(c, name, INT_MIN);
        if (v != INT_MIN) return v;
    }
    return fallback;
}

// 由 slice tree 经 pic_parameter_set_id → PPS → seq_parameter_set_id → SPS
// 找到所引用的 SPS 节点;查不到返回 nullptr。
const parser::SyntaxNode* spsForSlice(const parser::ParameterSetStore& ps,
                                      const parser::SyntaxNode& slice) {
    const int pps_id = findInt(slice, "pic_parameter_set_id", -1);
    const parser::SyntaxNode* pps = ps.findPPS(pps_id);
    if (pps == nullptr) return nullptr;
    const int sps_id = findInt(*pps, "seq_parameter_set_id", -1);
    return ps.findSPS(sps_id);
}

}  // namespace

bool StreamSession::open(const std::string& path) {
    es_.clear();
    frames_.clear();
    psets_.clear();

    // TODO(C.2 green):实现下面 5 步。
    //
    // 1) 读整个文件到 es_(二进制):
    //      std::ifstream f(path, std::ios::binary | std::ios::ate);
    //      if (!f) return false;
    //      const std::streamsize n = f.tellg();
    //      if (n <= 0) return false;
    //      es_.resize(static_cast<size_t>(n));
    //      f.seekg(0);
    //      f.read(reinterpret_cast<char*>(es_.data()), n);
    //
    // 2) 切 NAL:
    //      parser::NALSplitter splitter;
    //      auto nals = splitter.split(es_.data(), es_.size());
    //
    // 3) 准备解析器 + POC 计算器(POC 计算器懒构造):
    //      parser::H264SyntaxParser parser;
    //      parser.setParameterSets(&psets_);
    //      std::optional<POCCalculator> poc;   // #include <optional>
    //
    // 4) 遍历 nals,对每个 nal:
    //      const uint8_t* p = es_.data() + nal.payload_offset;
    //      auto rbsp = parser::stripEmulationPrevention(p, nal.size);
    //      auto tree = parser.parseNAL(rbsp.data(), rbsp.size(), nal.nal_unit_type);
    //
    //      若 type==7: int id = findInt(tree, "seq_parameter_set_id", 0);
    //                  psets_.addSPS(id, tree);
    //      若 type==8: int id = findInt(tree, "pic_parameter_set_id", 0);
    //                  psets_.addPPS(id, tree);
    //      若 type==1 或 5(slice):
    //          int first_mb = findInt(tree, "first_mb_in_slice", 0);
    //          if (first_mb != 0) {
    //              // 同一帧的后续 slice:并入当前帧(MVP fixture 不会走到)
    //              if (!frames_.empty()) {
    //                  frames_.back().nals.push_back(nal);
    //                  frames_.back().byte_size += nal.size;
    //              }
    //              continue; // 进下一个 nal
    //          }
    //          // first_mb==0 → 新帧
    //          FrameRecord fr;
    //          fr.index = static_cast<int>(frames_.size());
    //          int st_raw = findInt(tree, "slice_type", 2);   // "7 (I)" → 7
    //          fr.type = static_cast<SliceType>(st_raw % 5);
    //          bool is_idr = (nal.nal_unit_type == 5);
    //
    //          const parser::SyntaxNode* sps = spsForSlice(psets_, tree);
    //          int poc_type = sps ? findInt(*sps, "pic_order_cnt_type", 0) : 0;
    //          if (sps && poc_type == 0) {
    //              if (!poc) {
    //                  int log2 = findInt(*sps, "log2_max_pic_order_cnt_lsb_minus4", 0) + 4;
    //                  poc.emplace(log2);
    //              }
    //              int lsb = findInt(tree, "pic_order_cnt_lsb", 0);
    //              fr.poc = poc->compute(is_idr, lsb);
    //          } else {
    //              fr.poc = 0;  // POC type 1/2 或无 SPS:MVP 记 0
    //          }
    //
    //          fr.byte_offset_in_es = nal.byte_offset;
    //          fr.byte_size = (nal.payload_offset - nal.byte_offset) + nal.size;
    //          fr.nals.push_back(nal);
    //          fr.syntax_tree = tree;
    //          fr.missing_paramset = (sps == nullptr) || tree.incomplete;
    //          frames_.push_back(std::move(fr));
    //
    //      进度回调(可选):
    //          if (progress_) progress_(static_cast<int>((i + 1) * 100 / nals.size()));
    //
    // 5) return true;
    //
    // 提示:helper findInt / spsForSlice 已在本文件 anon namespace 给好,直接用。
    (void)path;
    (void)spsForSlice;
    (void)findInt;
    return false;
}

}  // namespace model
