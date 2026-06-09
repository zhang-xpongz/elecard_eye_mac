# `model::StreamSession::open` — 参考实现

> ⚠️ 自己实现完、7 个测试全绿后再看。

---

## 1. 角色:第一个聚合根

StreamSession 把 Phase B 全部零件组装成"打开文件 → 帧列表"。它是 domain 层对外
的主入口,UI 只跟它打交道。这一层能跑通,证明 parser/model 自洽、不依赖 UI/解码库。

流水线:
```
ifstream 读全文件 → es_
NALSplitter.split(es_) → nals
for nal in nals:
    strip EP3 → parseNAL
    7 → addSPS    8 → addPPS    1/5 → 切帧 + 算 POC
```

## 2. 完整参考实现

```cpp
#include "model/stream_session.h"

#include <climits>
#include <fstream>
#include <optional>
#include <utility>

#include "model/poc_calculator.h"
#include "parser/ep3_strip.h"
#include "parser/h264_syntax_parser.h"
#include "parser/nal_splitter.h"

namespace model {

namespace {

int findInt(const parser::SyntaxNode& n, const char* name, int fallback) {
    if (n.name == name) return std::stoi(n.value);   // "7 (I)" → 7
    for (const auto& c : n.children) {
        const int v = findInt(c, name, INT_MIN);
        if (v != INT_MIN) return v;
    }
    return fallback;
}

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

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    if (n <= 0) return false;
    es_.resize(static_cast<size_t>(n));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(es_.data()), n);

    parser::NALSplitter splitter;
    const auto nals = splitter.split(es_.data(), es_.size());

    parser::H264SyntaxParser parser;
    parser.setParameterSets(&psets_);
    std::optional<POCCalculator> poc;

    for (size_t i = 0; i < nals.size(); ++i) {
        const auto& nal = nals[i];
        const uint8_t* p = es_.data() + nal.payload_offset;
        const auto rbsp = parser::stripEmulationPrevention(p, nal.size);
        auto tree = parser.parseNAL(rbsp.data(), rbsp.size(), nal.nal_unit_type);

        if (nal.nal_unit_type == 7) {
            psets_.addSPS(findInt(tree, "seq_parameter_set_id", 0), tree);
        } else if (nal.nal_unit_type == 8) {
            psets_.addPPS(findInt(tree, "pic_parameter_set_id", 0), tree);
        } else if (nal.nal_unit_type == 1 || nal.nal_unit_type == 5) {
            const int first_mb = findInt(tree, "first_mb_in_slice", 0);
            if (first_mb != 0) {                 // 同帧的后续 slice
                if (!frames_.empty()) {
                    frames_.back().nals.push_back(nal);
                    frames_.back().byte_size += nal.size;
                }
            } else {                             // 新帧
                FrameRecord fr;
                fr.index = static_cast<int>(frames_.size());
                const int st_raw = findInt(tree, "slice_type", 2);
                fr.type = static_cast<SliceType>(st_raw % 5);
                const bool is_idr = (nal.nal_unit_type == 5);

                const parser::SyntaxNode* sps = spsForSlice(psets_, tree);
                const int poc_type = sps ? findInt(*sps, "pic_order_cnt_type", 0) : 0;
                if (sps && poc_type == 0) {
                    if (!poc) {
                        const int log2 =
                            findInt(*sps, "log2_max_pic_order_cnt_lsb_minus4", 0) + 4;
                        poc.emplace(log2);
                    }
                    fr.poc = poc->compute(is_idr, findInt(tree, "pic_order_cnt_lsb", 0));
                } else {
                    fr.poc = 0;
                }

                fr.byte_offset_in_es = nal.byte_offset;
                fr.byte_size = (nal.payload_offset - nal.byte_offset) + nal.size;
                fr.nals.push_back(nal);
                fr.syntax_tree = std::move(tree);
                fr.missing_paramset = (sps == nullptr) || fr.syntax_tree.incomplete;
                frames_.push_back(std::move(fr));
            }
        }

        if (progress_)
            progress_(static_cast<int>((i + 1) * 100 / nals.size()));
    }
    return true;
}

}  // namespace model
```

## 3. 期望值对照

| fixture | #帧 | type 序列 | POC 序列 |
|---|---|---|---|
| 01_idr_only | 5 | I I I I I | 0 0 0 0 0(POC type 2) |
| 02_ipbb | 8 | I P B B I P B B | 0 6 2 4 0 6 2 4 |

02 第 5 帧是第二个 IDR,POCCalculator 收到 is_idr=true 自动重置,所以 POC 回到 0。

## 4. 容易踩的坑

1. **`slice_type` 的 value 是 "7 (I)"**,不是纯数字。`std::stoi` 只读前导整数返回 7,
   再 `% 5` 得归一类型。直接 `== "I"` 之类是错的。

2. **POCCalculator 要跨帧存活**。它持有 prev_msb/prev_lsb 状态,必须一个实例贯穿
   整个 open()(用 `std::optional` 懒构造,首个 POC-type-0 slice 时建)。每帧 new 一个
   会丢状态,wrap-around 全错。IDR 的重置靠传 `is_idr=true`,不是重建对象。

3. **POC type 2 不建计算器**。fixture 01 是 type 2,`poc` optional 始终为空,所有帧
   poc=0。别无条件构造(会拿错 log2)。

4. **新帧判定看 `first_mb_in_slice==0`**。MVP fixture 一帧一 slice,每个都是 0;但
   规则要写对,否则将来多 slice 流会把一帧拆成多帧。

5. **每次 open 先 clear**。es_/frames_/psets_ 都要清,否则重复 open 残留旧数据
   (ReopenResetsState 测的就是这点)。

6. **进度回调到 100**。最后一个 nal 时 `(i+1)*100/size == 100`。整数除法,注意
   `(i+1)*100` 不要溢出(nals 数量小,无虞)。

## 5. byte_size 的定义(MVP)

`byte_size = (payload_offset - byte_offset) + size` = 起始码长度 + payload 字节。
即该 slice NAL 在 ES 中占的完整跨度(含起始码)。多 slice 帧会累加后续 slice 的
size。这是帧列表 "Size" 列的来源。更精确的"到下一帧起点"留作后续。

## 6. 后续(本 Task 不做)

- mmap 替换 ifstream(C.3 FileSource)
- MP4 输入(Phase G,经 demuxer 转 Annex-B)
- 多 slice/帧的完整聚合、AUD 分帧
- POC type 1/2 的精确计算
