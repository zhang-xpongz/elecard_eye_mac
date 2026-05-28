#include "parser/nal_splitter.h"

namespace parser {

std::vector<NALUnit> NALSplitter::split(const uint8_t* data, size_t size) {
    // 提示:
    //   1) 早返回:data==nullptr 或 size<3 → 返回空 vector
    //   2) 单次扫描,找所有 0x00 0x00 0x01 匹配位置
    //      - 匹配后跳过 3 字节避免重复 match
    //   3) 对每个匹配位置 i:
    //        - 若 i>0 且 data[i-1]==0x00 → 4-byte 起始码,byte_offset = i-1
    //          否则                       → 3-byte 起始码,byte_offset = i
    //        - payload_offset = i + 3
    //   4) 第二轮(或在第一轮里推进时)填 size:
    //        - 非最后一个 NAL:size = 下一个 NAL.byte_offset - 本 NAL.payload_offset
    //        - 最后一个 NAL:  size = buffer_size - payload_offset
    //   5) nal_unit_type:
    //        - 有 payload(payload_offset < size)→ data[payload_offset] & 0x1F
    //        - 否则                              → 0
    if (data == nullptr || size < 3) return {};
    std::vector<NALUnit> nals;
    for (size_t i = 0; i < size - 2; i++){
        if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1) {
            NALUnit nal;
            nal.byte_offset = (i > 0 && data[i-1] == 0) ? (i-1) : i;
            nal.payload_offset = i + 3;
            nals.push_back(nal);
            i += 2;
        }
    }

    for (size_t k = 0; k < nals.size(); k++) {
        nals[k].size = (k + 1 < nals.size()) ? (nals[k+1].byte_offset - nals[k].payload_offset) : (size - nals[k].payload_offset);
        nals[k].nal_unit_type = (nals[k].payload_offset < size) ? static_cast<uint8_t>(data[nals[k].payload_offset] & 0x1F) : 0;
    }
    return nals;
}

}  // namespace parser
