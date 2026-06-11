// dump_frames — Phase C 验收 CLI。
// 打开 .h264 裸流,打印每帧的 index / POC / 类型 / 字节大小。
//
//   dump_frames <file.h264>

#include <cstdio>

#include "model/stream_session.h"

namespace {

const char* sliceTypeName(model::SliceType t) {
    switch (t) {
        case model::SliceType::P:  return "P";
        case model::SliceType::B:  return "B";
        case model::SliceType::I:  return "I";
        case model::SliceType::SP: return "SP";
        case model::SliceType::SI: return "SI";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <file.h264>\n", argv[0]);
        return 1;
    }

    model::StreamSession session;
    if (!session.open(argv[1])) {
        std::fprintf(stderr, "Failed to open: %s\n", argv[1]);
        return 2;
    }

    const auto& frames = session.frames();
    std::printf("%-6s  %-6s  %-6s  %-10s  %s\n",
                "index", "POC", "type", "size", "offset");
    std::printf("------  ------  ------  ----------  ----------\n");
    for (const auto& f : frames) {
        std::printf("%-6d  %-6d  %-6s  %-10zu  %zu%s\n",
                    f.index, f.poc, sliceTypeName(f.type), f.byte_size,
                    f.byte_offset_in_es,
                    f.missing_paramset ? "  [missing paramset]" : "");
    }
    std::printf("\n%zu frames, ES %zu bytes\n", frames.size(), session.esSize());
    return 0;
}
