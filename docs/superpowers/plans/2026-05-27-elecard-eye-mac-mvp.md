# Elecard Eye Mac — MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从零搭出一个 macOS 桌面应用,能打开 H.264 裸流/MP4,在四宫格界面里展示帧列表、SPS/PPS/SliceHeader 语法树、字节级 Hex 视图(可联动高亮)、以及解码后的画面预览。

**Architecture:** Qt 6 GUI + 纯 C++17 自研 parser + FFmpeg 解码,通过共享 `StreamSession` 解耦。Parser/Model 是 DDD 中的 domain 层(零外部依赖),Decoder/IO 是 infrastructure,App 是 presentation。

**Tech Stack:** C++17, CMake 3.20+, Qt 6.5+, FFmpeg 6.0+, GoogleTest 1.14, macOS 13+.

**学习模式约定**:本计划采用"骨架 + 对照参考"风格。
- **源码文件**:仅给接口签名/类骨架,函数体留空 `// TODO: implement to pass tests`,由你亲手实现
- **测试文件**:**完整代码**(TDD 锚点,先红再绿)
- **每个源码文件配一份 reference doc**:`docs/superpowers/reference/<path>-reference.md`,内含完整参考实现 + 关键决策注释,**事后对照学习**
- **DDD**:统一使用 H.264 规范术语;domain 层(libs/model + libs/parser)绝不持有 FFmpeg/Qt 类型
- **TDD 铁律**:每个 Task 顺序为 写测试 → 跑红 → 写实现 → 跑绿 → commit。不允许跳

---

## Phases 概览

| Phase | 主题 | 产出 | 可演示性 |
|---|---|---|---|
| A | 项目脚手架 | 空 Qt 窗口能跑,`ctest` 能跑通 hello test | 启动一个 Hello 窗口 |
| B | 解析器基础(纯 C++) | BitReader + NALSplitter + SPS/PPS/SliceHeader parser | CLI 把 `.h264` 解析成 JSON,无 GUI |
| C | Domain Model | StreamSession 能开 `.h264` 列帧 | CLI `dump-frames foo.h264` 输出帧表 |
| D | GUI 骨架 + 帧列表 + 语法树 | 双面板 Qt App,能打开 `.h264` 看到帧列表和语法树 | **MVP 可见雏形** |
| E | Hex Viewer + 联动 | 第三个面板,语法树点击 → Hex 高亮 | 全键盘导航跑通 |
| F | FFmpeg Decoder + Preview Panel | 第四个面板,解码后画面显示 | **MVP 五件套完成** |
| G | MP4 支持 | StreamSession 自动识别 mp4 + Annex-B 提取 | 双格式齐全 |
| H | 烟雾测试 + .app 打包 | QTest 烟雾用例 + dmg | 可分发 |

---

## File Structure(全局)

```
elecard_eye_mac/
├── CMakeLists.txt
├── cmake/{FindFFmpeg.cmake, Sanitizers.cmake}
├── libs/
│   ├── io/
│   │   ├── include/io/file_source.h
│   │   ├── src/file_source.cpp
│   │   ├── tests/file_source_test.cpp
│   │   └── CMakeLists.txt
│   ├── parser/
│   │   ├── include/parser/{bit_reader.h, syntax_node.h, nal_unit.h,
│   │   │   nal_splitter.h, h264_syntax_parser.h, i_syntax_parser.h}
│   │   ├── src/{bit_reader.cpp, nal_splitter.cpp, h264_syntax_parser.cpp}
│   │   ├── tests/{bit_reader_test.cpp, nal_splitter_test.cpp,
│   │   │   h264_sps_test.cpp, h264_pps_test.cpp, h264_slice_header_test.cpp}
│   │   └── CMakeLists.txt
│   ├── model/
│   │   ├── include/model/{frame_record.h, parameter_set_store.h, stream_session.h}
│   │   ├── src/stream_session.cpp
│   │   ├── tests/stream_session_test.cpp
│   │   └── CMakeLists.txt
│   └── decoder/
│       ├── include/decoder/{decoded_frame.h, ffmpeg_decoder.h}
│       ├── src/ffmpeg_decoder.cpp
│       ├── tests/ffmpeg_decoder_test.cpp
│       └── CMakeLists.txt
├── app/
│   ├── main.cpp
│   ├── main_window.{h,cpp}
│   ├── panels/
│   │   ├── frame_list_panel.{h,cpp}
│   │   ├── frame_list_model.{h,cpp}
│   │   ├── syntax_tree_panel.{h,cpp}
│   │   ├── syntax_tree_model.{h,cpp}
│   │   ├── hex_viewer_panel.{h,cpp}
│   │   └── preview_panel.{h,cpp}
│   ├── tests/smoke_test.cpp
│   └── CMakeLists.txt
├── tools/
│   └── dump_frames/main.cpp                # Phase C CLI 演示
├── tests/
│   ├── fixtures/                            # CMake 生成的 .h264/.mp4(不入仓)
│   └── gen_fixtures.sh                      # ffmpeg 调用脚本
└── docs/
    ├── superpowers/specs/2026-05-27-elecard-eye-mac-design.md
    ├── superpowers/plans/2026-05-27-elecard-eye-mac-mvp.md
    └── superpowers/reference/               # 每个源码文件的参考实现 doc
        ├── libs/...
        └── app/...
```

---

# Phase A — 项目脚手架

### Task A.1: 顶层 CMakeLists + 编译选项

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Sanitizers.cmake`

- [ ] **Step 1: 写顶层 `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(elecard_eye_mac CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(APPLE)
  add_compile_options(-Wno-deprecated-declarations)
endif()

include(cmake/Sanitizers.cmake)

# Qt
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Test)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                  libavformat libavcodec libavutil libswscale)

# GoogleTest
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_subdirectory(libs/io)
add_subdirectory(libs/parser)
add_subdirectory(libs/model)
add_subdirectory(libs/decoder)
add_subdirectory(app)
add_subdirectory(tools/dump_frames)
```

- [ ] **Step 2: 写 `cmake/Sanitizers.cmake`**

```cmake
option(ENABLE_ASAN "Enable AddressSanitizer in Debug" ON)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer in Debug" ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
  if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()
```

- [ ] **Step 3: 验证 cmake configure 通过**

(尚无子目录,会因为缺 subdir 失败 — 暂留,完成 A.2 后再测)

- [ ] **Step 4: 创建 reference doc**

`docs/superpowers/reference/CMakeLists-reference.md` — 详述每个选项的作用、为什么 `-Werror`、为什么 sanitizer 默认开等。

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/ docs/superpowers/reference/
git commit -m "build: scaffold top-level CMake with Qt/FFmpeg/GTest discovery"
```

---

### Task A.2: hello-world 测试 target

**Files:**
- Create: `libs/io/CMakeLists.txt`(占位)
- Create: `libs/io/include/io/.gitkeep`
- Create: `libs/io/tests/hello_test.cpp`

**目的**:验证 GoogleTest 集成跑通。

- [ ] **Step 1: 写 `libs/io/CMakeLists.txt` 占位**

```cmake
add_library(streameye_io INTERFACE)
target_include_directories(streameye_io INTERFACE include)

if(BUILD_TESTING)
  add_executable(io_tests tests/hello_test.cpp)
  target_link_libraries(io_tests PRIVATE streameye_io GTest::gtest_main)
  gtest_discover_tests(io_tests)
endif()
```

- [ ] **Step 2: 写 hello 测试(完整)**

```cpp
// libs/io/tests/hello_test.cpp
#include <gtest/gtest.h>

TEST(HelloTest, Sanity) {
    EXPECT_EQ(2 + 2, 4);
}
```

- [ ] **Step 3: 类似占位 4 个子库 CMakeLists**

为 `libs/parser`、`libs/model`、`libs/decoder`、`app`、`tools/dump_frames` 各写一个最小 `CMakeLists.txt`(只声明 INTERFACE/STATIC,无源码):

```cmake
# libs/parser/CMakeLists.txt(类似 io)
add_library(streameye_parser INTERFACE)
target_include_directories(streameye_parser INTERFACE include)
```

`app/CMakeLists.txt`:
```cmake
# 暂时不构建,Phase D 启用
# add_executable(streameye_app main.cpp)
```

`tools/dump_frames/CMakeLists.txt`:
```cmake
# Phase C 启用
```

(完整代码见 reference doc)

- [ ] **Step 4: 配置 + 构建 + 跑测试**

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `HelloTest.Sanity` PASS。

- [ ] **Step 5: 创建 reference doc**

`docs/superpowers/reference/libs/io/CMakeLists-reference.md`,描述 `gtest_discover_tests` vs `add_test()` 的取舍。

- [ ] **Step 6: Commit**

```bash
git add libs/ app/ tools/ docs/superpowers/reference/
git commit -m "build: stub all subdir CMakeLists, hello gtest passes"
```

---

### Task A.3: 测试 fixture 生成脚本

**Files:**
- Create: `tests/gen_fixtures.sh`
- Create: `tests/CMakeLists.txt`

**目的**:用 ffmpeg CLI 生成 4 个测试 `.h264`/`.mp4` 到 `build/fixtures/`,不入 git。

- [ ] **Step 1: 写 `tests/gen_fixtures.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
OUT="${1:?usage: gen_fixtures.sh <out_dir>}"
mkdir -p "$OUT"

# 01: 5 IDR-only frames, Baseline, 128x128
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=5:duration=1" \
  -c:v libx264 -profile:v baseline -g 1 -bf 0 \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/01_idr_only.h264"

# 02: 8 frames IPBBPBBI, Main, Open GOP
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=8:duration=1" \
  -c:v libx264 -profile:v main -g 4 -bf 2 \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/02_ipbb.h264"

# 03: 16 frames with B refs, High
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=16:duration=1" \
  -c:v libx264 -profile:v high -g 8 -bf 3 -refs 4 \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/03_with_b_refs.h264"

# 04: mp4 container wrapping 02
ffmpeg -y -i "$OUT/02_ipbb.h264" -c copy -f mp4 "$OUT/04_short.mp4"
```

- [ ] **Step 2: 写 `tests/CMakeLists.txt`** 自动调用脚本

```cmake
set(FIXTURES_DIR ${CMAKE_BINARY_DIR}/fixtures)
add_custom_command(
  OUTPUT ${FIXTURES_DIR}/01_idr_only.h264
         ${FIXTURES_DIR}/02_ipbb.h264
         ${FIXTURES_DIR}/03_with_b_refs.h264
         ${FIXTURES_DIR}/04_short.mp4
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gen_fixtures.sh ${FIXTURES_DIR}
  DEPENDS gen_fixtures.sh
  COMMENT "Generating test fixtures via ffmpeg"
)
add_custom_target(fixtures ALL DEPENDS ${FIXTURES_DIR}/01_idr_only.h264)
```

把 `add_subdirectory(tests)` 加到顶层 `CMakeLists.txt`(在 `enable_testing()` 之后)。

- [ ] **Step 3: chmod + 验证**

```bash
chmod +x tests/gen_fixtures.sh
cmake --build build --target fixtures
ls build/fixtures/
```

Expected: 4 个文件存在,大小 > 0。

- [ ] **Step 4: Reference doc**

`docs/superpowers/reference/tests/gen_fixtures-reference.md`,记录 ffmpeg 各参数含义、为什么选 testsrc、profile 取舍。

- [ ] **Step 5: Commit**

```bash
git add tests/ CMakeLists.txt docs/superpowers/reference/
git commit -m "test: add ffmpeg-driven fixture generation (idr-only, ipbb, b-refs, mp4)"
```

---

# Phase B — Parser 基础(纯 C++)

DDD 笔记:这一阶段产出的所有类型/函数都属于 **domain layer**,严禁 include Qt/FFmpeg/任何 OS 头(POSIX 也不行)。BitReader/NALSplitter 是纯算法,字节 in、结构 out。

### Task B.1: `BitReader` — 跨字节位读取

**Files:**
- Create: `libs/parser/include/parser/bit_reader.h` (骨架)
- Create: `libs/parser/src/bit_reader.cpp` (骨架)
- Create: `libs/parser/tests/bit_reader_test.cpp` (完整)
- Create: `docs/superpowers/reference/libs/parser/bit_reader-reference.md` (完整参考)

- [ ] **Step 1: 写完整测试**

```cpp
// libs/parser/tests/bit_reader_test.cpp
#include <gtest/gtest.h>
#include "parser/bit_reader.h"

using parser::BitReader;

TEST(BitReader, ReadOneBitAtATime) {
    const uint8_t data[] = {0b10110100};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 1u);
    EXPECT_EQ(r.readBits(1), 0u);
    EXPECT_EQ(r.readBits(1), 0u);
}

TEST(BitReader, ReadAcrossByteBoundary) {
    const uint8_t data[] = {0xAB, 0xCD};  // 1010 1011 1100 1101
    BitReader r(data, 2);
    EXPECT_EQ(r.readBits(4), 0xAu);
    EXPECT_EQ(r.readBits(8), 0xBCu);
    EXPECT_EQ(r.readBits(4), 0xDu);
}

TEST(BitReader, BitOffsetTracking) {
    const uint8_t data[] = {0xFF, 0xFF};
    BitReader r(data, 2);
    EXPECT_EQ(r.bitOffset(), 0u);
    r.readBits(3);
    EXPECT_EQ(r.bitOffset(), 3u);
    r.readBits(7);
    EXPECT_EQ(r.bitOffset(), 10u);
}

TEST(BitReader, UnsignedExpGolomb) {
    // 1 = '010', 2 = '011', 3 = '00100', 4 = '00101', 7 = '0001000'
    const uint8_t data[] = {0b01001100, 0b10000101, 0b00010000};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readUE(), 1u);
    EXPECT_EQ(r.readUE(), 2u);
    EXPECT_EQ(r.readUE(), 3u);
    EXPECT_EQ(r.readUE(), 4u);
    EXPECT_EQ(r.readUE(), 7u);
}

TEST(BitReader, SignedExpGolomb) {
    // se(v): 0→0, 1→1, 2→-1, 3→2, 4→-2 ...
    const uint8_t data[] = {0b10100110, 0b01000000};
    BitReader r(data, sizeof(data));
    EXPECT_EQ(r.readSE(), 0);
    EXPECT_EQ(r.readSE(), 1);
    EXPECT_EQ(r.readSE(), -1);
    EXPECT_EQ(r.readSE(), 2);
}

TEST(BitReader, OutOfRangeSetsErrorFlag) {
    const uint8_t data[] = {0xFF};
    BitReader r(data, 1);
    r.readBits(8);
    EXPECT_FALSE(r.hasError());
    r.readBits(1);  // 越界
    EXPECT_TRUE(r.hasError());
}
```

- [ ] **Step 2: 跑测试,确认全红**

```bash
cmake --build build --target parser_tests
ctest --test-dir build -R BitReader --output-on-failure
```

Expected: 编译失败 — `parser/bit_reader.h` 不存在。

- [ ] **Step 3: 写头文件骨架**

```cpp
// libs/parser/include/parser/bit_reader.h
#pragma once
#include <cstdint>
#include <cstddef>

namespace parser {

// MSB-first bit reader over a contiguous byte buffer.
// Supports H.264 unsigned/signed Exp-Golomb (ue/se).
// On out-of-range read, sets internal error flag instead of throwing.
class BitReader {
public:
    BitReader(const uint8_t* data, size_t size_bytes);

    // 读 n 个 bit(MSB-first),n ∈ [1, 32]
    uint32_t readBits(int n);

    // ue(v) — H.264 9.1
    uint32_t readUE();

    // se(v) — H.264 9.1.1
    int32_t  readSE();

    // 当前 bit 位置(从开头算)
    size_t   bitOffset() const;

    bool     hasError() const;

private:
    // TODO: implement to pass tests
    // 提示:用一个 uint64_t 缓冲 + bit cursor 实现最简单;
    //       或者直接 (data[off/8] >> (7 - off%8)) & 1 一位一位读
    const uint8_t* data_;
    size_t  size_bytes_;
    size_t  bit_offset_ = 0;
    bool    error_ = false;
};

}  // namespace parser
```

- [ ] **Step 4: 写 cpp 骨架**

```cpp
// libs/parser/src/bit_reader.cpp
#include "parser/bit_reader.h"

namespace parser {

BitReader::BitReader(const uint8_t* data, size_t size_bytes)
    : data_(data), size_bytes_(size_bytes) {}

uint32_t BitReader::readBits(int /*n*/) {
    // TODO: implement
    return 0;
}

uint32_t BitReader::readUE() {
    // TODO: implement using readBits
    return 0;
}

int32_t BitReader::readSE() {
    // TODO: implement using readUE
    return 0;
}

size_t BitReader::bitOffset() const { return bit_offset_; }
bool   BitReader::hasError() const  { return error_; }

}  // namespace parser
```

- [ ] **Step 5: 更新 `libs/parser/CMakeLists.txt`**

```cmake
add_library(streameye_parser STATIC
  src/bit_reader.cpp
)
target_include_directories(streameye_parser PUBLIC include)
target_compile_features(streameye_parser PUBLIC cxx_std_17)

if(BUILD_TESTING)
  add_executable(parser_tests tests/bit_reader_test.cpp)
  target_link_libraries(parser_tests PRIVATE streameye_parser GTest::gtest_main)
  gtest_discover_tests(parser_tests)
endif()
```

- [ ] **Step 6: 写参考实现 doc**

`docs/superpowers/reference/libs/parser/bit_reader-reference.md` 内含完整可粘贴实现 + 设计笔记(为什么 MSB-first、Exp-Golomb 算法步骤示意、错误标志而非异常的理由)。

- [ ] **Step 7: 用户实现 BitReader,跑测试到全绿**

```bash
cmake --build build --target parser_tests
ctest --test-dir build -R BitReader --output-on-failure
```

Expected: 6 个 test 全部 PASS。

- [ ] **Step 8: Commit**

```bash
git add libs/parser/ docs/superpowers/reference/libs/parser/bit_reader-reference.md
git commit -m "feat(parser): BitReader with ue/se and bit-offset tracking"
```

---

### Task B.2: `NALSplitter` — Annex-B 起始码扫描

**Files:**
- Create: `libs/parser/include/parser/{nal_unit.h, nal_splitter.h}`
- Create: `libs/parser/src/nal_splitter.cpp`
- Create: `libs/parser/tests/nal_splitter_test.cpp`
- Create: `docs/superpowers/reference/libs/parser/nal_splitter-reference.md`

- [ ] **Step 1: 写完整测试**

```cpp
// libs/parser/tests/nal_splitter_test.cpp
#include <gtest/gtest.h>
#include "parser/nal_splitter.h"

using parser::NALSplitter;

TEST(NALSplitter, ThreeByteStartCode) {
    // 起始码 00 00 01 + nal_header 0x67 (SPS) + payload
    const uint8_t buf[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].byte_offset, 0u);
    EXPECT_EQ(nals[0].payload_offset, 3u);
    EXPECT_EQ(nals[0].size, 4u);
    EXPECT_EQ(nals[0].nal_unit_type, 0x07u);  // SPS
}

TEST(NALSplitter, FourByteStartCode) {
    const uint8_t buf[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].byte_offset, 0u);
    EXPECT_EQ(nals[0].payload_offset, 4u);
    EXPECT_EQ(nals[0].size, 4u);
}

TEST(NALSplitter, MultipleNALs) {
    const uint8_t buf[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F,    // NAL 1 (SPS)
        0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x06, 0xE2,    // NAL 2 (PPS)
        0x00, 0x00, 0x01, 0x65, 0x88, 0x80, 0x10           // NAL 3 (IDR slice, 3-byte sc)
    };
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 3u);
    EXPECT_EQ(nals[0].nal_unit_type, 7u);  // SPS
    EXPECT_EQ(nals[1].nal_unit_type, 8u);  // PPS
    EXPECT_EQ(nals[2].nal_unit_type, 5u);  // IDR
}

TEST(NALSplitter, EmulationPreventionDoesNotSplit) {
    // payload 里有 00 00 03 不是起始码,应该不被错切
    const uint8_t buf[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x00, 0x00, 0x03, 0x01, 0xFF,
    };
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].size, 6u);
}

TEST(NALSplitter, TruncatedTrailingBytes) {
    // 文件末尾不完整,起始码后只有 1 byte
    const uint8_t buf[] = {0x00, 0x00, 0x00, 0x01, 0x67};
    auto nals = NALSplitter{}.split(buf, sizeof(buf));
    ASSERT_EQ(nals.size(), 1u);
    EXPECT_EQ(nals[0].size, 1u);
}

TEST(NALSplitter, EmptyBuffer) {
    NALSplitter s;
    EXPECT_TRUE(s.split(nullptr, 0).empty());
}
```

- [ ] **Step 2: 写头文件骨架**

```cpp
// libs/parser/include/parser/nal_unit.h
#pragma once
#include <cstdint>
#include <cstddef>
namespace parser {
struct NALUnit {
    size_t  byte_offset;     // 起始码开头(0x00 00 00 01 / 0x00 00 01)
    size_t  payload_offset;  // 跳过起始码后的位置
    size_t  size;            // payload 字节数(含 nal_header byte)
    uint8_t nal_unit_type;   // payload[0] & 0x1F
};
}
```

```cpp
// libs/parser/include/parser/nal_splitter.h
#pragma once
#include "parser/nal_unit.h"
#include <vector>

namespace parser {
class NALSplitter {
public:
    std::vector<NALUnit> split(const uint8_t* data, size_t size);
};
}
```

- [ ] **Step 3: 写 cpp 骨架**

```cpp
// libs/parser/src/nal_splitter.cpp
#include "parser/nal_splitter.h"

namespace parser {
std::vector<NALUnit> NALSplitter::split(const uint8_t* /*data*/, size_t /*size*/) {
    // TODO: implement
    // 提示:扫 00 00 01 / 00 00 00 01;两个起始码之间是一段 NAL;
    //       最后一段从最后一个起始码到 buf 末尾
    return {};
}
}
```

- [ ] **Step 4: 更新 CMakeLists**

把 `src/nal_splitter.cpp` 加入 `streameye_parser` STATIC,把 `tests/nal_splitter_test.cpp` 加入 `parser_tests`。

- [ ] **Step 5: 写 reference doc**

`docs/superpowers/reference/libs/parser/nal_splitter-reference.md`:完整实现 + 关于"为什么不需要处理 EP3 反转义"(那是 RBSP 的事,Splitter 只切边界)。

- [ ] **Step 6: 用户实现,测试全绿**

```bash
cmake --build build --target parser_tests
ctest --test-dir build -R NALSplitter --output-on-failure
```

Expected: 6 tests PASS.

- [ ] **Step 7: Commit**

```bash
git add libs/parser/ docs/superpowers/reference/libs/parser/nal_splitter-reference.md
git commit -m "feat(parser): NALSplitter with 3/4-byte start codes and truncation tolerance"
```

---

### Task B.3: `SyntaxNode` 数据结构 + `EP3` RBSP 反转义工具

**Files:**
- Create: `libs/parser/include/parser/syntax_node.h`
- Create: `libs/parser/include/parser/ep3_strip.h`
- Create: `libs/parser/src/ep3_strip.cpp`
- Create: `libs/parser/tests/ep3_strip_test.cpp`
- Reference docs for each

- [ ] **Step 1: 写 `SyntaxNode` 头文件(无需测试 — 纯 POD)**

```cpp
// libs/parser/include/parser/syntax_node.h
#pragma once
#include <string>
#include <vector>
#include <cstddef>
namespace parser {
struct SyntaxNode {
    std::string name;            // "seq_parameter_set_rbsp" / "level_idc"
    std::string value;           // 渲染后的值字符串
    size_t      bit_offset = 0;  // 在 ES 中的全局 bit 偏移(用于 Hex 高亮)
    size_t      bit_length = 0;
    bool        incomplete = false;
    std::vector<SyntaxNode> children;
};
}
```

- [ ] **Step 2: 写 `ep3_strip` 测试(完整)**

```cpp
// libs/parser/tests/ep3_strip_test.cpp
#include <gtest/gtest.h>
#include "parser/ep3_strip.h"

using parser::stripEmulationPrevention;

TEST(EP3Strip, NoEscapesUnchanged) {
    const std::vector<uint8_t> in  = {0x67, 0x42, 0xC0, 0x1F};
    auto out = stripEmulationPrevention(in.data(), in.size());
    EXPECT_EQ(out, in);
}

TEST(EP3Strip, SingleEscapeRemoved) {
    // 00 00 03 01 → 00 00 01
    const std::vector<uint8_t> in  = {0x67, 0x00, 0x00, 0x03, 0x01};
    const std::vector<uint8_t> exp = {0x67, 0x00, 0x00, 0x01};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}

TEST(EP3Strip, MultipleEscapes) {
    const std::vector<uint8_t> in  = {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x01};
    const std::vector<uint8_t> exp = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}

TEST(EP3Strip, EscapeAtEnd) {
    const std::vector<uint8_t> in  = {0xFF, 0x00, 0x00, 0x03};
    const std::vector<uint8_t> exp = {0xFF, 0x00, 0x00};
    EXPECT_EQ(stripEmulationPrevention(in.data(), in.size()), exp);
}
```

- [ ] **Step 3: 写 `ep3_strip.h` 骨架**

```cpp
// libs/parser/include/parser/ep3_strip.h
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace parser {
// H.264 7.4.1.1: 将 00 00 03 XX → 00 00 XX(XX ∈ {00,01,02,03})
std::vector<uint8_t> stripEmulationPrevention(const uint8_t* data, size_t size);
}
```

- [ ] **Step 4: 写 `ep3_strip.cpp` 骨架**

```cpp
// libs/parser/src/ep3_strip.cpp
#include "parser/ep3_strip.h"
namespace parser {
std::vector<uint8_t> stripEmulationPrevention(const uint8_t* /*data*/, size_t /*size*/) {
    // TODO: implement
    return {};
}
}
```

- [ ] **Step 5: 更新 CMake,实现到绿,reference doc,commit**

(参考实现见 `docs/superpowers/reference/libs/parser/ep3_strip-reference.md`)

```bash
git add libs/parser/ docs/superpowers/reference/libs/parser/
git commit -m "feat(parser): SyntaxNode and EP3 (emulation prevention) byte strip"
```

---

### Task B.4: `H264SyntaxParser::parseSPS` (最小 SPS 字段集)

**Files:**
- Create: `libs/parser/include/parser/{i_syntax_parser.h, h264_syntax_parser.h}`
- Create: `libs/parser/src/h264_syntax_parser.cpp`
- Create: `libs/parser/tests/h264_sps_test.cpp`
- Reference doc

**MVP SPS 字段范围**:`profile_idc`, `constraint_set_flags`, `level_idc`, `seq_parameter_set_id`, `chroma_format_idc`(High Profile), `bit_depth_luma_minus8`, `bit_depth_chroma_minus8`, `log2_max_frame_num_minus4`, `pic_order_cnt_type`, `log2_max_pic_order_cnt_lsb_minus4`(type 0), `max_num_ref_frames`, `gaps_in_frame_num_value_allowed_flag`, `pic_width_in_mbs_minus1`, `pic_height_in_map_units_minus1`, `frame_mbs_only_flag`, `direct_8x8_inference_flag`, `frame_cropping_flag` + crop offsets, `vui_parameters_present_flag`。

完整 VUI、scaling list **不在 MVP**。

- [ ] **Step 1: 写测试(选取 Baseline + High 两个 SPS)**

```cpp
// libs/parser/tests/h264_sps_test.cpp
#include <gtest/gtest.h>
#include "parser/h264_syntax_parser.h"
#include "parser/ep3_strip.h"

using parser::H264SyntaxParser;
using parser::SyntaxNode;

// Helper: find child node by name (深度优先,返回第一个)
static const SyntaxNode* find(const SyntaxNode& n, const std::string& name) {
    if (n.name == name) return &n;
    for (auto& c : n.children) {
        if (auto* hit = find(c, name)) return hit;
    }
    return nullptr;
}

TEST(H264SPS, BaselineProfile128x128) {
    // x264 Baseline, 128x128, 来自 fixture 01 的实际 SPS RBSP(EP3 已剥)
    // 字节由参考 doc 给出;以下示意
    const uint8_t rbsp[] = {
        0x42, 0xC0, 0x1F, 0xD9, 0x00, 0x8B, 0x4B, 0x20, /* ... */
    };
    auto stripped = parser::stripEmulationPrevention(rbsp, sizeof(rbsp));
    H264SyntaxParser p;
    auto tree = p.parseNAL(stripped.data(), stripped.size(), /*nal_unit_type=*/7);

    EXPECT_EQ(find(tree, "profile_idc")->value, "66");
    EXPECT_EQ(find(tree, "level_idc")->value, "31");
    EXPECT_EQ(find(tree, "pic_width_in_mbs_minus1")->value, "7");   // 128/16 - 1
    EXPECT_EQ(find(tree, "pic_height_in_map_units_minus1")->value, "7");
    EXPECT_EQ(find(tree, "frame_mbs_only_flag")->value, "1");
}

TEST(H264SPS, HighProfileHas4x4Chroma) {
    const uint8_t rbsp[] = { /* fixture 03 SPS */ };
    auto stripped = parser::stripEmulationPrevention(rbsp, sizeof(rbsp));
    H264SyntaxParser p;
    auto tree = p.parseNAL(stripped.data(), stripped.size(), 7);
    EXPECT_EQ(find(tree, "profile_idc")->value, "100");
    EXPECT_NE(find(tree, "chroma_format_idc"), nullptr);  // High 才有
    EXPECT_EQ(find(tree, "chroma_format_idc")->value, "1");
}

TEST(H264SPS, BitOffsetsAreContiguous) {
    const uint8_t rbsp[] = { /* fixture 01 SPS */ };
    auto stripped = parser::stripEmulationPrevention(rbsp, sizeof(rbsp));
    H264SyntaxParser p;
    auto tree = p.parseNAL(stripped.data(), stripped.size(), 7);
    // 第一个字段的 bit_offset 应该是 8(跳过 nal_unit_header 一个 byte)
    auto* first = &tree.children[0];
    EXPECT_EQ(first->bit_offset, 8u);
}
```

> 注:测试里的 SPS 字节由 reference doc 提供完整版本(从 fixture `.h264` 抽出的真实字节)。

- [ ] **Step 2: 写头文件骨架**

```cpp
// libs/parser/include/parser/i_syntax_parser.h
#pragma once
#include "parser/syntax_node.h"
#include <cstdint>
#include <cstddef>
namespace parser {
class ISyntaxParser {
public:
    virtual ~ISyntaxParser() = default;
    // 输入:RBSP(EP3 已剥) + nal_unit_type
    // 输出:语法树根节点(name = "nal_unit_<n>_<NAME>")
    virtual SyntaxNode parseNAL(const uint8_t* rbsp, size_t size,
                                uint8_t nal_unit_type) = 0;
};
}
```

```cpp
// libs/parser/include/parser/h264_syntax_parser.h
#pragma once
#include "parser/i_syntax_parser.h"
namespace parser {
class H264SyntaxParser : public ISyntaxParser {
public:
    SyntaxNode parseNAL(const uint8_t* rbsp, size_t size,
                        uint8_t nal_unit_type) override;
private:
    // Phase B 只实现 SPS;PPS/SliceHeader 在后续 task
    SyntaxNode parseSPS(const uint8_t* rbsp, size_t size);
};
}
```

- [ ] **Step 3: 写 cpp 骨架(SPS 字段一个个 TODO)**

```cpp
// libs/parser/src/h264_syntax_parser.cpp
#include "parser/h264_syntax_parser.h"
#include "parser/bit_reader.h"

namespace parser {

SyntaxNode H264SyntaxParser::parseNAL(const uint8_t* rbsp, size_t size,
                                      uint8_t nal_unit_type) {
    if (nal_unit_type == 7) return parseSPS(rbsp, size);
    // PPS / SliceHeader 留给后续 task
    return SyntaxNode{};
}

SyntaxNode H264SyntaxParser::parseSPS(const uint8_t* /*rbsp*/, size_t /*size*/) {
    SyntaxNode root{"seq_parameter_set_rbsp", "", 0, 0, false, {}};
    // TODO: 用 BitReader 顺序读字段,每读一个 push_back 一个 SyntaxNode 到 root.children
    //
    // 字段顺序(H.264 7.3.2.1.1):
    //   u(8)  profile_idc
    //   u(1)  constraint_set0_flag .. set5_flag
    //   u(2)  reserved_zero_2bits
    //   u(8)  level_idc
    //   ue(v) seq_parameter_set_id
    //   if (profile in {100,110,122,244,44,83,86,118,128,138,139,134,135}) {
    //     ue(v) chroma_format_idc
    //     if (chroma_format_idc == 3) u(1) separate_colour_plane_flag
    //     ue(v) bit_depth_luma_minus8
    //     ue(v) bit_depth_chroma_minus8
    //     u(1)  qpprime_y_zero_transform_bypass_flag
    //     u(1)  seq_scaling_matrix_present_flag (假设为 0,不解 scaling list)
    //   }
    //   ue(v) log2_max_frame_num_minus4
    //   ue(v) pic_order_cnt_type
    //   if (pic_order_cnt_type == 0) ue(v) log2_max_pic_order_cnt_lsb_minus4
    //   ue(v) max_num_ref_frames
    //   u(1)  gaps_in_frame_num_value_allowed_flag
    //   ue(v) pic_width_in_mbs_minus1
    //   ue(v) pic_height_in_map_units_minus1
    //   u(1)  frame_mbs_only_flag
    //   if (!frame_mbs_only_flag) u(1) mb_adaptive_frame_field_flag
    //   u(1)  direct_8x8_inference_flag
    //   u(1)  frame_cropping_flag
    //   if (frame_cropping_flag) { ue(v) ×4 crop offsets }
    //   u(1)  vui_parameters_present_flag (不解 vui,MVP 范围外)
    return root;
}

}  // namespace parser
```

- [ ] **Step 4: 更新 CMake + 写 reference doc + 用户实现 + 跑绿 + commit**

Reference doc:`docs/superpowers/reference/libs/parser/h264_sps-reference.md`,提供:
- 每个字段的完整代码(含 `bit_offset` 记录)
- 真实 fixture SPS 字节(从 `build/fixtures/*.h264` 用 `xxd` 抽出来贴上)
- 一个 helper `addField(node, name, value, bits_consumed)` 的建议实现

```bash
git add libs/parser/ docs/superpowers/reference/libs/parser/
git commit -m "feat(parser): H.264 SPS parser (subset: no VUI, no scaling list)"
```

---

### Task B.5: `H264SyntaxParser::parsePPS`

**Files:**
- Modify: `libs/parser/src/h264_syntax_parser.cpp`(加 `parsePPS`)
- Modify: `libs/parser/include/parser/h264_syntax_parser.h`
- Create: `libs/parser/tests/h264_pps_test.cpp`
- Reference doc

**MVP PPS 字段**:`pic_parameter_set_id`, `seq_parameter_set_id`, `entropy_coding_mode_flag`, `bottom_field_pic_order_in_frame_present_flag`, `num_slice_groups_minus1`(若 >0 直接 abort,MVP 不支持 FMO), `num_ref_idx_l0/l1_default_active_minus1`, `weighted_pred_flag`, `weighted_bipred_idc`, `pic_init_qp_minus26`, `pic_init_qs_minus26`, `chroma_qp_index_offset`, `deblocking_filter_control_present_flag`, `constrained_intra_pred_flag`, `redundant_pic_cnt_present_flag`。

(详细测试代码 + 字段顺序见 reference doc)

- [ ] **Step 1-7**:与 B.4 结构相同(测试 → 骨架 → 实现 → 绿 → commit)

```bash
git commit -m "feat(parser): H.264 PPS parser (subset: no FMO, no scaling list extension)"
```

---

### Task B.6: `H264SyntaxParser::parseSliceHeader`

**MVP Slice Header 字段**(参考 H.264 7.3.3):`first_mb_in_slice`, `slice_type`, `pic_parameter_set_id`, `frame_num`, `field_pic_flag`(若 frame_mbs_only_flag=0), `idr_pic_id`(IDR), `pic_order_cnt_lsb`(POC type 0), `redundant_pic_cnt`, `num_ref_idx_active_override_flag` (...), `slice_qp_delta`, `disable_deblocking_filter_idc`。

**MVP 不解**:reference picture list modification、weighted prediction、dec_ref_pic_marking 详细字段。这些标"NOT PARSED, see future iteration"。

**关键依赖**:解析 Slice Header 需要查找对应的 SPS+PPS,所以 parser 需要支持 `setParameterSets(const ParameterSetStore*)`。

**DDD 决策**:`ParameterSetStore` 放在 **`parser::` 命名空间**(`libs/parser/include/parser/parameter_set_store.h`),因为它存的是 `parser::SyntaxNode` 解析树,概念上是 parser 的"已解析头部仓储"。这与 spec §3.2 把它列在 Stream Model 部分有偏差 — 取舍是让 parser 不依赖 model,符合"domain 子层各自独立可测"的更严约束。Model 层后续(C.1 已无 ParameterSetStore Task,直接引用 parser 的即可)使用同一类型。

> ⚠️ Plan 与 spec 的差异已记录:ParameterSetStore 命名空间 `parser::`(plan)vs Stream Model 表格(spec)。原因如上。

**新增 Task 顺序**:这一 Task 因此**拆为 B.6a(ParameterSetStore)+ B.6b(SliceHeader)**,以保证类型可用先于使用。

#### B.6a — `parser::ParameterSetStore`

**Files:**
- Create: `libs/parser/include/parser/parameter_set_store.h`
- Create: `libs/parser/src/parameter_set_store.cpp`
- Create: `libs/parser/tests/parameter_set_store_test.cpp`
- Reference doc

测试覆盖 addSPS/findSPS/addPPS/findPPS/clear,接口:

```cpp
// libs/parser/include/parser/parameter_set_store.h
#pragma once
#include "parser/syntax_node.h"
#include <unordered_map>
namespace parser {
class ParameterSetStore {
public:
    void addSPS(int id, SyntaxNode tree);
    void addPPS(int id, SyntaxNode tree);
    const SyntaxNode* findSPS(int id) const;
    const SyntaxNode* findPPS(int id) const;
    void clear();
private:
    std::unordered_map<int, SyntaxNode> sps_;
    std::unordered_map<int, SyntaxNode> pps_;
};
}
```

测试、骨架、实现、reference doc、commit 流程同前。

```bash
git commit -m "feat(parser): ParameterSetStore for SPS/PPS lookup by id"
```

#### B.6b — `parseSliceHeader`

- [ ] **Step 1: 完整测试**

```cpp
// libs/parser/tests/h264_slice_header_test.cpp
#include <gtest/gtest.h>
#include "parser/h264_syntax_parser.h"
#include "parser/parameter_set_store.h"

TEST(H264SliceHeader, IDRSlice) {
    // 从 fixture 01 抽一个 IDR slice 的字节(EP3 已剥)
    const uint8_t rbsp[] = { /* see reference doc */ };

    parser::ParameterSetStore psets;
    // 测试里先 parse 出 SPS/PPS 塞进 store(或直接用预制好的 SyntaxNode)
    parser::H264SyntaxParser p;
    p.setParameterSets(&psets);
    auto tree = p.parseNAL(rbsp, size, /*nal_unit_type=*/5);
    EXPECT_EQ(find(tree, "slice_type")->value, "7 (I)");
    EXPECT_EQ(find(tree, "frame_num")->value, "0");
    EXPECT_EQ(find(tree, "idr_pic_id")->value, "0");
}

TEST(H264SliceHeader, PSliceWithRefs) {
    // ... fixture 02 的第二帧
}

TEST(H264SliceHeader, BSlice) {
    // ... fixture 02 的某 B 帧
}
```

- [ ] **Step 2-7**:同 B.4

```bash
git commit -m "feat(parser): H.264 SliceHeader parser (subset, depends on SPS/PPS lookup)"
```

---

# Phase C — Domain Model

DDD 笔记:这一阶段 `StreamSession` 是 application service / aggregate root。它协调 NALSplitter + H264SyntaxParser + ParameterSetStore,产出 `FrameRecord` 集合。**仍然零 Qt / FFmpeg 依赖** — 这一层能跑通才保证 domain 干净。

### Task C.1: `FrameRecord` + 简单 POC 计算

> 注:`ParameterSetStore` 已在 B.6a 完成(`parser::`),Model 层直接 `#include "parser/parameter_set_store.h"` 使用。

**Files:**
- Create: `libs/model/include/model/frame_record.h`
- Create: `libs/model/include/model/poc_calculator.h` + `src/poc_calculator.cpp`
- Create: `libs/model/tests/poc_calculator_test.cpp`
- Reference docs

**POC 计算范围(MVP)**:仅 `pic_order_cnt_type = 0`。POC type 1/2 标 TODO。

- [ ] **Step 1: `FrameRecord` 头(POD,无测试)**

```cpp
// libs/model/include/model/frame_record.h
#pragma once
#include "parser/nal_unit.h"
#include "parser/syntax_node.h"
#include "parser/parameter_set_store.h"  // 复用 parser 的 store
#include <vector>

namespace model {
enum class SliceType { P=0, B=1, I=2, SP=3, SI=4 };  // 简化:%5 之后的值

struct FrameRecord {
    int      index = 0;        // 解码顺序
    int      poc = 0;          // 显示顺序(POC type 0)
    SliceType type = SliceType::I;
    size_t   byte_offset_in_es = 0;
    size_t   byte_size = 0;
    std::vector<parser::NALUnit> nals;
    parser::SyntaxNode syntax_tree;
    bool     missing_paramset = false;
};
}
```

- [ ] **Step 2: POC calculator 测试**

```cpp
// libs/model/tests/poc_calculator_test.cpp
#include <gtest/gtest.h>
#include "model/poc_calculator.h"

using model::POCCalculator;

TEST(POC, IDRResetsState) {
    POCCalculator c(/*log2_max_pic_order_cnt_lsb=*/4);  // max_lsb=16
    EXPECT_EQ(c.compute(/*is_idr=*/true, /*pic_order_cnt_lsb=*/0), 0);
}

TEST(POC, IncrementingLSB) {
    POCCalculator c(4);
    c.compute(true, 0);
    EXPECT_EQ(c.compute(false, 2), 2);
    EXPECT_EQ(c.compute(false, 4), 4);
    EXPECT_EQ(c.compute(false, 6), 6);
}

TEST(POC, LSBWrapAroundIncreasesMSB) {
    POCCalculator c(4);  // max_lsb=16
    c.compute(true, 0);
    c.compute(false, 8);
    c.compute(false, 14);
    // 下一个 lsb=2 应该被识别为 wrap → poc=18
    EXPECT_EQ(c.compute(false, 2), 18);
}
```

- [ ] **Step 3-7**:头骨架、实现、reference doc、commit

参考 H.264 8.2.1.1 算法。

```bash
git commit -m "feat(model): POC computation for pic_order_cnt_type=0"
```

---

### Task C.2: `StreamSession::open` — 打开 `.h264` 裸流

**Files:**
- Create: `libs/model/include/model/stream_session.h`
- Create: `libs/model/src/stream_session.cpp`
- Create: `libs/model/tests/stream_session_test.cpp`
- Reference doc

**MVP 范围**:这一 Task 只处理 `.h264` 裸流(从磁盘 mmap)。MP4 留给 Phase G。

- [ ] **Step 1: 测试 — 用 fixture 文件**

```cpp
#include <gtest/gtest.h>
#include "model/stream_session.h"

TEST(StreamSession, OpenIdrOnlyHasFiveFrames) {
    model::StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/01_idr_only.h264"));
    EXPECT_EQ(s.frames().size(), 5u);
    for (auto& f : s.frames())
        EXPECT_EQ(f.type, model::SliceType::I);
}

TEST(StreamSession, OpenIPBBHasMixedTypes) {
    model::StreamSession s;
    ASSERT_TRUE(s.open(FIXTURES_DIR "/02_ipbb.h264"));
    EXPECT_EQ(s.frames().size(), 8u);
    EXPECT_EQ(s.frames()[0].type, model::SliceType::I);
    // POC 顺序应与显示顺序一致
    EXPECT_LT(s.frames()[0].poc, s.frames()[1].poc + 100); // 简化断言
}

TEST(StreamSession, OpenNonexistentFails) {
    model::StreamSession s;
    EXPECT_FALSE(s.open("/no/such/file.h264"));
}
```

CMake 需要把 `FIXTURES_DIR` 作为编译宏传给 model_tests:
```cmake
target_compile_definitions(model_tests PRIVATE
    FIXTURES_DIR=\"${CMAKE_BINARY_DIR}/fixtures\")
add_dependencies(model_tests fixtures)
```

- [ ] **Step 2: 头骨架**

```cpp
// libs/model/include/model/stream_session.h
#pragma once
#include "model/frame_record.h"
#include "parser/parameter_set_store.h"
#include <functional>
#include <string>
#include <vector>

namespace model {
class StreamSession {
public:
    bool open(const std::string& path);

    const std::vector<FrameRecord>& frames() const;
    const parser::ParameterSetStore& parameterSets() const;
    const uint8_t*                  esData() const;
    size_t                          esSize() const;

    using ProgressCb = std::function<void(int percent)>;
    void setProgressCallback(ProgressCb cb);

private:
    // TODO
};
}
```

- [ ] **Step 3: cpp 骨架 + 7 步流程**

```cpp
// libs/model/src/stream_session.cpp
#include "model/stream_session.h"
#include "io/file_source.h"  // 见 Task C.4 — 先用 std::ifstream 占位
// ...
bool StreamSession::open(const std::string& path) {
    // TODO 实现步骤:
    // 1) 读整个文件到 es_buffer_(MVP 用 ifstream;Task C.4 替换为 mmap)
    // 2) NALSplitter 切出 NALUnit 列表
    // 3) 遍历 NAL:
    //      type=7  → parser.parseSPS → ParameterSetStore.addSPS
    //      type=8  → parser.parsePPS → ParameterSetStore.addPPS
    //      type∈{1,5} → parser.parseSliceHeader → 新建/追加 FrameRecord
    //                   (first_mb_in_slice=0 起新帧;否则附加到当前帧)
    // 4) POCCalculator 给每个 FrameRecord 算 poc
    // 5) 进度回调
    return false;
}
```

- [ ] **Step 4-7**:CMake、reference doc、实现、绿灯、commit

```bash
git commit -m "feat(model): StreamSession.open() for raw .h264 (ifstream-backed)"
```

---

### Task C.3: `FileSource` — mmap 替换 ifstream

**Files:**
- Create: `libs/io/include/io/file_source.h`
- Create: `libs/io/src/file_source.cpp`
- Create: `libs/io/tests/file_source_test.cpp`
- Reference doc
- Modify: `libs/model/src/stream_session.cpp` 用 FileSource

**Domain 边界**:`FileSource` 属于 infrastructure(用 POSIX `mmap`),通过头文件被 model 引用。这是允许的"infrastructure → domain"方向。

- [ ] **Step 1-7**:测试(打开 fixture 文件断言大小 + 字节)、骨架、实现、reference doc、commit

```cpp
TEST(FileSource, MmapReadsBytes) {
    io::FileSource fs;
    ASSERT_TRUE(fs.openMmap(FIXTURES_DIR "/01_idr_only.h264"));
    ASSERT_GT(fs.size(), 0u);
    EXPECT_EQ(fs.data()[0], 0x00);  // Annex-B 起始码
    EXPECT_EQ(fs.data()[1], 0x00);
}

TEST(FileSource, NonexistentReturnsFalse) {
    io::FileSource fs;
    EXPECT_FALSE(fs.openMmap("/no/such"));
}
```

```bash
git commit -m "feat(io): FileSource mmap implementation, StreamSession uses it"
```

---

### Task C.4: CLI 工具 `dump_frames` — Phase C 验收

**Files:**
- Create: `tools/dump_frames/main.cpp`
- Modify: `tools/dump_frames/CMakeLists.txt`

**目的**:在没有 GUI 的情况下验证 Phase B+C 全部能跑。这是 Phase C 的可演示产出。

- [ ] **Step 1: 写测试 — 这里直接用集成测试**

```cpp
// 因为 main 不好直接测,集成测试用 system() 调用 + 解析 stdout
// 放在 tools/dump_frames/tests/integration_test.cpp(可选);
// 或者直接手工跑 + 给个 expected.txt 文件做 diff
```

简化:这个 Task 不做严格 TDD,但产出后必须人工 demo 一次。

- [ ] **Step 2: 写 main.cpp 骨架**

```cpp
// tools/dump_frames/main.cpp
#include "model/stream_session.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <file.h264>\n", argv[0]);
        return 1;
    }
    model::StreamSession s;
    if (!s.open(argv[1])) {
        std::fprintf(stderr, "Failed to open: %s\n", argv[1]);
        return 2;
    }
    // TODO: 打印每帧 index/poc/type/size
    return 0;
}
```

- [ ] **Step 3: CMake 启用 target**
- [ ] **Step 4: reference doc**
- [ ] **Step 5: 人工 demo**

```bash
cmake --build build
./build/tools/dump_frames/dump_frames build/fixtures/02_ipbb.h264
# Expected: 8 行,各帧类型/POC
```

- [ ] **Step 6: Commit**

```bash
git commit -m "tools: add dump_frames CLI as Phase C demo"
```

---

# Phase D — GUI 骨架 + Frame List + Syntax Tree

### Task D.1: Qt MainWindow shell

**Files:**
- Create: `app/main.cpp`
- Create: `app/main_window.{h,cpp}`
- Modify: `app/CMakeLists.txt`

- [ ] **Step 1: 测试 — 用 QTest 验证窗口能创建**

```cpp
// app/tests/main_window_test.cpp
#include <QApplication>
#include <gtest/gtest.h>
#include "main_window.h"

TEST(MainWindow, CreatesWithoutCrash) {
    int argc = 0;
    QApplication app(argc, nullptr);
    MainWindow w;
    EXPECT_EQ(w.windowTitle(), "Elecard Eye Mac");
}
```

- [ ] **Step 2-7**:骨架、CMake(启用 Qt::AUTOMOC、链 Widgets)、实现、reference doc、commit

```bash
git commit -m "feat(app): Qt MainWindow shell with File→Open action"
```

---

### Task D.2: `FrameListModel` + `FrameListPanel`

**Files:**
- Create: `app/panels/{frame_list_model.h, frame_list_model.cpp}`
- Create: `app/panels/{frame_list_panel.h, frame_list_panel.cpp}`
- Create: `app/tests/frame_list_model_test.cpp`
- Reference docs

`FrameListModel` 继承 `QAbstractTableModel`,4 列:Index、POC、Type、Size。

- [ ] **Step 1: 测试 model 行数/数据**

```cpp
TEST(FrameListModel, EmptyWhenNoSession) {
    FrameListModel m;
    EXPECT_EQ(m.rowCount(), 0);
    EXPECT_EQ(m.columnCount(), 4);
}

TEST(FrameListModel, ShowsFramesFromSession) {
    model::StreamSession s;
    s.open(FIXTURES_DIR "/02_ipbb.h264");
    FrameListModel m;
    m.setSession(&s);
    EXPECT_EQ(m.rowCount(), 8);
    auto idx = m.index(0, 2);  // Type 列
    EXPECT_EQ(m.data(idx).toString(), "I");
}
```

- [ ] **Step 2-7**:骨架、Panel(QTableView wrapping model)、reference doc、commit

```bash
git commit -m "feat(app): FrameListModel and FrameListPanel"
```

---

### Task D.3: `SyntaxTreeModel` + `SyntaxTreePanel`

**Files:**
- Create: `app/panels/{syntax_tree_model.h, syntax_tree_model.cpp}`
- Create: `app/panels/{syntax_tree_panel.h, syntax_tree_panel.cpp}`
- Create: `app/tests/syntax_tree_model_test.cpp`
- Reference docs

`SyntaxTreeModel` 继承 `QAbstractItemModel`,递归展示 `SyntaxNode`(2 列:Name / Value)。**Selection 模型要 emit 一个 `nodeSelected(const parser::SyntaxNode*)` 信号**(供 Hex 联动用)。

- [ ] **Step 1: 测试**

```cpp
TEST(SyntaxTreeModel, RecursesIntoChildren) {
    parser::SyntaxNode root{"root", "", 0, 0, false,
        { {"a", "1", 0, 8, false, {}}, {"b", "2", 8, 8, false, {}} }};
    SyntaxTreeModel m;
    m.setRoot(root);
    EXPECT_EQ(m.rowCount(QModelIndex()), 1);  // root
    auto rootIdx = m.index(0, 0, QModelIndex());
    EXPECT_EQ(m.rowCount(rootIdx), 2);        // a, b
}
```

- [ ] **Step 2-7**:同 D.2

```bash
git commit -m "feat(app): SyntaxTreeModel and SyntaxTreePanel with selection signal"
```

---

### Task D.4: MainWindow 装配 — File→Open → 两个面板联动

**Files:**
- Modify: `app/main_window.cpp`(把 D.2/D.3 的 Panel 装进 QDockWidget,接上 File→Open)
- Create: `app/session_loader.{h,cpp}`(worker thread 封装,见下方)

**关键**:`StreamSession::open()` 不能在 UI 线程跑(可能秒级)。要在 QThread 里跑,完成后用 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 回主线程更新 model。

- [ ] **Step 1: 测试 SessionLoader 异步行为**

```cpp
TEST(SessionLoader, LoadsAndEmitsReady) {
    int argc = 0; QApplication app(argc, nullptr);
    SessionLoader loader;
    QSignalSpy spy(&loader, &SessionLoader::sessionReady);
    loader.loadAsync(FIXTURES_DIR "/01_idr_only.h264");
    ASSERT_TRUE(spy.wait(5000));
    EXPECT_EQ(spy.count(), 1);
}
```

- [ ] **Step 2-7**:骨架、Connection 接线、reference doc、commit

```bash
git commit -m "feat(app): async session loader, MainWindow wires FrameList<->SyntaxTree"
```

> **Phase D 验收**:能跑 `streameye_app`,打开 `build/fixtures/02_ipbb.h264`,左边面板看到 8 帧,点一帧右边面板出现 SPS/PPS/SliceHeader 树。

---

# Phase E — Hex Viewer + 联动

### Task E.1: `HexViewerPanel`(纯绘制,自定义 QAbstractScrollArea)

**Files:**
- Create: `app/panels/{hex_viewer_panel.h, hex_viewer_panel.cpp}`
- Create: `app/tests/hex_viewer_panel_test.cpp`
- Reference doc

**核心实现要点**:
- 16 字节一行,左侧 offset(8 hex)+ 中间 hex bytes + 右侧 ASCII
- 用 `paintEvent` 只绘制可见行(virtual scroll)
- 暴露 `setHighlight(size_t byte_start, size_t byte_end)` 用于联动

- [ ] **Step 1: 测试 — 用 `QImage` 渲染快照 + 像素 sanity**

```cpp
TEST(HexViewerPanel, RendersOffsetsAndBytes) {
    HexViewerPanel p;
    std::vector<uint8_t> data = {0x67, 0x42, 0xC0, 0x1F};
    p.setData(data.data(), data.size());
    p.resize(800, 600);
    QImage img(p.size(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    p.render(&img);
    // 不做严格像素比对;简单 sanity:图片非全白
    bool hasNonWhite = false;
    for (int y = 0; y < img.height() && !hasNonWhite; y += 10)
      for (int x = 0; x < img.width() && !hasNonWhite; x += 10)
        if (img.pixel(x, y) != qRgb(255, 255, 255)) hasNonWhite = true;
    EXPECT_TRUE(hasNonWhite);
}
```

- [ ] **Step 2-7**:骨架、绘制实现、reference doc、commit

```bash
git commit -m "feat(app): HexViewerPanel with virtual scrolling and highlight range"
```

---

### Task E.2: SyntaxTree → Hex 联动

**Files:**
- Modify: `app/main_window.cpp` — 把 SyntaxTreePanel 的 `nodeSelected` 信号接到 HexViewerPanel
- 涉及 `SyntaxNode.bit_offset / bit_length` → byte 范围换算

- [ ] **Step 1: 测试**

```cpp
TEST(MainWindowIntegration, SyntaxSelectionHighlightsHex) {
    // 创建 MainWindow,加载 fixture,模拟选中某语法节点,
    // 断言 HexViewerPanel 的 highlight 范围正确
    // (可能需要把 HexViewerPanel 暴露成 testable getter)
}
```

- [ ] **Step 2-7**:接线、reference doc、commit

```bash
git commit -m "feat(app): wire SyntaxTree selection to HexViewer highlight"
```

> **Phase E 验收**:点击语法树里 `level_idc`,Hex 面板自动滚动到对应字节并高亮。

---

# Phase F — FFmpeg Decoder + Preview Panel

### Task F.1: `DecodedFrame` + `FFmpegDecoder` 接口

**Files:**
- Create: `libs/decoder/include/decoder/{decoded_frame.h, ffmpeg_decoder.h}`
- Create: `libs/decoder/src/ffmpeg_decoder.cpp`
- Create: `libs/decoder/tests/ffmpeg_decoder_test.cpp`
- Reference doc
- Modify: `libs/decoder/CMakeLists.txt` — link PkgConfig::FFMPEG

**Domain 边界**:`DecodedFrame` 是纯 std 类型(`std::vector<uint8_t>`,无 `AVFrame*`),这是 anti-corruption layer。

- [ ] **Step 1: 测试 — 喂 fixture,解第一帧,断言尺寸**

```cpp
TEST(FFmpegDecoder, DecodesFirstFrameOfIdrOnly) {
    model::StreamSession s;
    s.open(FIXTURES_DIR "/01_idr_only.h264");
    decoder::FFmpegDecoder dec;
    ASSERT_TRUE(dec.init(s.parameterSets(), s.esData(), s.esSize()));
    decoder::DecodedFrame f;
    ASSERT_TRUE(dec.decodeFrame(0, f));
    EXPECT_EQ(f.width, 128);
    EXPECT_EQ(f.height, 128);
    EXPECT_EQ(f.y_plane.size(), 128u * 128u);
}

TEST(FFmpegDecoder, RandomAccessRestartsAtIDR) {
    // 测试 fixture 02 第 5 帧能正确解出
}
```

- [ ] **Step 2-7**:接口骨架、FFmpeg 调用实现、reference doc、commit

> **Reference doc 必须给完整 FFmpeg 调用代码**(`avcodec_alloc_context3` / `avcodec_send_packet` / `avcodec_receive_frame` 的正确顺序),因为这是 FFmpeg 的痛点。

```bash
git commit -m "feat(decoder): FFmpegDecoder with random-access (restart from nearest IDR)"
```

---

### Task F.2: `PreviewPanel` — YUV → QImage 显示

**Files:**
- Create: `app/panels/{preview_panel.h, preview_panel.cpp}`
- Create: `app/tests/preview_panel_test.cpp`
- Reference doc

- [ ] **Step 1: 测试**

```cpp
TEST(PreviewPanel, RendersYUVAsImage) {
    PreviewPanel p;
    decoder::DecodedFrame f;
    f.width = 16; f.height = 16;
    f.y_plane.assign(256, 128);
    f.u_plane.assign(64, 128);
    f.v_plane.assign(64, 128);
    // 中性灰
    p.setFrame(f);
    p.resize(200, 200);
    QImage out(p.size(), QImage::Format_ARGB32);
    out.fill(Qt::black);
    p.render(&out);
    // 中间应该是灰色而不是黑色
    EXPECT_NE(out.pixel(100, 100), qRgb(0, 0, 0));
}
```

- [ ] **Step 2-7**:骨架(YUV420P → RGB 转换可用 libswscale,见 reference doc)、commit

```bash
git commit -m "feat(app): PreviewPanel renders DecodedFrame via swscale->QImage"
```

---

### Task F.3: MainWindow 接入 Decoder + 异步解码线程

**Files:**
- Modify: `app/main_window.cpp`
- Create: `app/decoder_worker.{h,cpp}`

- [ ] **Step 1: 测试 worker 异步行为**

(参考 D.4 SessionLoader 测试结构,QSignalSpy 等待 `frameDecoded` 信号)

- [ ] **Step 2-7**:接线 — Frame 选中 → DecoderWorker → PreviewPanel.setFrame

```bash
git commit -m "feat(app): DecoderWorker thread, PreviewPanel updates on frame selection"
```

> **Phase F 验收 = MVP 完成**:打开 `02_ipbb.h264`,点任意帧,左/右/下三个面板有数据,中上 PreviewPanel 显示解码后的 128×128 测试图。

---

# Phase G — MP4 容器支持

### Task G.1: FFmpeg demuxer 提取 Annex-B ES

**Files:**
- Create: `libs/decoder/include/decoder/mp4_demuxer.h`
- Create: `libs/decoder/src/mp4_demuxer.cpp`
- Create: `libs/decoder/tests/mp4_demuxer_test.cpp`
- Reference doc

- [ ] **Step 1: 测试**

```cpp
TEST(MP4Demuxer, ExtractsAnnexBFromMp4) {
    decoder::MP4Demuxer d;
    std::vector<uint8_t> es;
    ASSERT_TRUE(d.extractH264AnnexB(FIXTURES_DIR "/04_short.mp4", es));
    // 第一个起始码
    ASSERT_GE(es.size(), 4u);
    EXPECT_EQ(es[0], 0x00);
    EXPECT_EQ(es[1], 0x00);
    EXPECT_TRUE(es[2] == 0x01 || (es[2] == 0x00 && es[3] == 0x01));
}
```

- [ ] **Step 2-7**:实现(用 `av_bsf_get_by_name("h264_mp4toannexb")`),commit

```bash
git commit -m "feat(decoder): MP4Demuxer extracts Annex-B ES via bitstream filter"
```

---

### Task G.2: `StreamSession::open` 自动识别 mp4

**Files:**
- Modify: `libs/model/src/stream_session.cpp`

**DDD 决策**:model 层现在被迫依赖 `libs/decoder` 的 demuxer,**这破坏分层**。解决方案:在 model 层定义 `IDemuxer` 接口,在 app 层注入具体实现(依赖反转)。

- [ ] **Step 1: 定义 model 层 `IDemuxer` 接口 + 测试用 stub**

```cpp
// libs/model/include/model/i_demuxer.h
namespace model {
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    virtual bool extractH264AnnexB(const std::string& path,
                                   std::vector<uint8_t>& out) = 0;
};
}
```

- [ ] **Step 2: `StreamSession::open` 改造**

```cpp
// 加构造参数 setDemuxer(IDemuxer*) 或 ctor 注入
// 文件后缀是 .mp4/.ts/.mkv 时调 demuxer,否则 mmap
```

- [ ] **Step 3: 测试**

```cpp
TEST(StreamSession, OpenMp4ViaDemuxer) {
    model::StreamSession s;
    decoder::MP4Demuxer d;  // 测试里允许跨层
    s.setDemuxer(&d);
    ASSERT_TRUE(s.open(FIXTURES_DIR "/04_short.mp4"));
    EXPECT_GT(s.frames().size(), 0u);
}
```

- [ ] **Step 4: app 层在 main 里注入**
- [ ] **Step 5-7**:reference doc + commit

```bash
git commit -m "feat(model): StreamSession accepts IDemuxer for .mp4 via dependency injection"
```

---

# Phase H — 烟雾测试 + 打包

### Task H.1: 端到端 QTest 烟雾测试

**Files:**
- Create: `app/tests/smoke_test.cpp`

- [ ] **Step 1: 写测试**

```cpp
TEST(Smoke, OpenFixtureFlow) {
    int argc = 0; QApplication app(argc, nullptr);
    MainWindow w;
    w.show();
    w.openFile(FIXTURES_DIR "/02_ipbb.h264");
    // 等待 sessionReady
    QSignalSpy spy(&w, &MainWindow::sessionReady);
    ASSERT_TRUE(spy.wait(5000));
    EXPECT_GT(w.frameListPanel()->model()->rowCount(), 0);
    // 选第一帧 → PreviewPanel 应有图
    w.selectFrame(0);
    QTest::qWait(500);
    EXPECT_FALSE(w.previewPanel()->currentImage().isNull());
}
```

- [ ] **Step 2-5**:骨架、暴露 testable accessor、commit

```bash
git commit -m "test(app): end-to-end smoke test for open→decode→display flow"
```

---

### Task H.2: macOS `.app` bundle 打包

**Files:**
- Modify: `app/CMakeLists.txt`

- [ ] **Step 1: 加 `MACOSX_BUNDLE` 属性**

```cmake
add_executable(streameye_app MACOSX_BUNDLE
  main.cpp main_window.cpp panels/...
)
set_target_properties(streameye_app PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "Elecard Eye Mac"
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.zhangxiaopeng.elecard-eye-mac"
  MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
)
```

- [ ] **Step 2: 用 `macdeployqt` 打包**

```bash
cmake --build build --target streameye_app
macdeployqt build/app/streameye_app.app -dmg
```

- [ ] **Step 3: 验证 dmg 能在另一个用户目录下打开运行**
- [ ] **Step 4: reference doc(签名、公证留作 future work)**
- [ ] **Step 5: Commit**

```bash
git commit -m "build: package as .app bundle, macdeployqt creates dmg"
```

---

## MVP Definition of Done

- [ ] 所有 Phase A-H 的 Task 全部 checkbox 完成
- [ ] `ctest --test-dir build` 全绿
- [ ] Debug build 在 ASan/UBSan 下跑测试零报错
- [ ] 手工测试:在新机器上 `brew install qt@6 ffmpeg cmake` → `cmake -B build && cmake --build build` → 双击 `build/app/streameye_app.app` → 打开 `01_idr_only.h264` 和 `04_short.mp4` 均能看到帧列表 + 语法树 + Hex + 解码预览
- [ ] 每个源码文件在 `docs/superpowers/reference/` 下有对应 reference doc

---

## 后续迭代提示(MVP 之后,本计划不覆盖)

- 宏块/CU 级可视化(`H264SyntaxParser::parseSliceData`)
- H.265/AV1/VVC parser 各起一个 `IXxxxSyntaxParser` 实现
- GOP 时间轴 panel
- 多帧 thumbnail 缓存
- 质量指标(PSNR/SSIM)
- CI 集成

每一项可以作为独立的 spec → plan → 实施循环。
