# Elecard Eye Mac — MVP Design

**Date:** 2026-05-27
**Status:** Draft for user review
**Purpose:** macOS 端 H.264 码流分析器(对标 Elecard StreamEye),练手项目兼具学习 codec 解析内核与 macOS 工程的双重目标。

---

## 1. 目标与范围

### 项目定位
- **学习导向**:既要吃透 H.264/H.265/AV1/VVC 的码流结构,又要做出工程化的 macOS 应用
- **长期愿景**:覆盖主流 codec(H.264/H.265/AV1/VVC),功能逼近 Elecard StreamEye Studio
- **MVP 边界**:仅 H.264,核心五件套(开文件 + 帧列表 + 语法树 + Hex + 解码预览)

### MVP 成功标准
1. 可在 macOS 13+ 上原生运行(Qt 6 桌面应用)
2. 能打开 `.h264` 裸流和 `.mp4` 封装文件
3. 帧列表显示:帧序号、POC、帧类型(I/P/B)、字节大小
4. 语法树展示 SPS / PPS / Slice Header 的完整字段
5. Hex 查看器与语法树联动:点击字段高亮对应字节
6. 解码预览:点击帧显示解码后的图像
7. 全部单元测试通过,Debug 构建在 ASan/UBSan 下零错误

### 非目标(MVP 明确不做)
- 编辑/保存码流(只读分析器)
- 实时网络流(仅本地文件)
- 多文件对比、撤销重做、设置持久化、国际化
- 宏块层及以下的可视化(MV/QP 热图/SAO 边界等)— 留给后续迭代

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                       Qt 6 GUI Layer                         │
│  MainWindow │ Dockable Panels │ Qt Model/View Adapters       │
└─────┬──────────────────────┬────────────────────────┬───────┘
      │ SyntaxNode tree      │ Frame metadata         │ QImage
      │ + bit offsets        │ list                   │ frame
      │                      │                        │
┌─────▼──────────┐   ┌───────▼──────────┐   ┌─────────▼───────┐
│  Parser Core   │   │   Stream Model   │   │  Decoder Core   │
│  (own C++17)   │◄──┤  (shared state)  │──►│  (FFmpeg)       │
│                │   │                  │   │                 │
│ NAL splitter   │   │ - Frame index    │   │ libavformat     │
│ SPS/PPS parser │   │ - SPS/PPS cache  │   │ libavcodec      │
│ SliceHeader    │   │ - byte offsets   │   │ libavutil       │
│ SyntaxTree     │   │ - decode state   │   │                 │
└─────┬──────────┘   └────────┬─────────┘   └─────────┬───────┘
      │                       │                       │
      └───────────────────────┴───────────────────────┘
                              │
                     ┌────────▼──────────┐
                     │  File I/O Layer   │
                     │  (mmap / fread)   │
                     └───────────────────┘
```

### 分层职责

| 层 | 关键类型 | 依赖 |
|---|---|---|
| GUI | `MainWindow`, `*Panel`, Qt model adapters | Qt 6 |
| Stream Model | `StreamSession`, `FrameRecord`, `ParameterSetStore` | 仅 C++17 标准库 |
| Parser Core | `NALSplitter`, `H264SyntaxParser`, `BitReader` | **仅 C++17 标准库,无第三方依赖** |
| Decoder Core | `FFmpegDecoder`, `Demuxer` | FFmpeg(libavformat/avcodec/avutil/swscale) |
| File I/O | `FileSource` | C++17 标准库 + POSIX mmap |

### 关键边界设计

- **Parser 不依赖 FFmpeg、不依赖 Qt**:纯 C++17 静态库,可独立 unit test,后期可搬到嵌入式
- **Stream Model 不依赖 Qt**:GUI 层只是其观察者;未来做 CLI 工具可直接复用
- **Parser 与 Decoder 不互相调用**:通过 Stream Model 解耦,各自处理同一段 Annex-B 字节流,以"byte offset / NAL index"对齐
- **app 是唯一同时引用 model + parser + decoder 的层**

---

## 3. 核心组件

### 3.1 Parser Core (`libstreameye_parser.a`)

```cpp
// NAL 切割 — Annex-B 起始码扫描
class NALSplitter {
public:
    struct NALUnit {
        size_t byte_offset;       // 在 ES 中的偏移(含起始码)
        size_t payload_offset;    // 跳过起始码后的偏移
        size_t size;              // payload 字节数
        uint8_t nal_unit_type;
    };
    std::vector<NALUnit> split(const uint8_t* data, size_t size);
};

// 通用语法树节点
struct SyntaxNode {
    std::string name;             // 如 "seq_parameter_set_rbsp"
    std::string value;            // 渲染后的值字符串
    size_t bit_offset;            // 起始 bit 偏移(用于 Hex 高亮)
    size_t bit_length;
    std::vector<SyntaxNode> children;
    bool incomplete = false;      // 截断标记
};

// H.264 解析器(后续 codec 各自实现同接口)
class ISyntaxParser {
public:
    virtual ~ISyntaxParser() = default;
    virtual SyntaxNode parseNAL(const uint8_t* rbsp, size_t size,
                                uint8_t nal_unit_type) = 0;
};

class H264SyntaxParser : public ISyntaxParser {
    // MVP: 实现 SPS / PPS / Slice Header
    // 未来: 加 SEI / VUI / Slice Data(MB 层)
};
```

**字段统一用 `SyntaxNode` 通用结构表达**(name + value + bit_offset),不为每个字段做专门 struct。代价:程序化访问要按名查;收益:UI 渲染统一、扩展性强。

### 3.2 Stream Model (`libstreameye_model.a`)

```cpp
struct FrameRecord {
    int index;                              // 解码顺序
    int poc;                                // 显示顺序
    SliceType type;                         // I / P / B
    size_t byte_offset_in_es;
    size_t byte_size;
    std::vector<NALSplitter::NALUnit> nals;
    SyntaxNode syntax_tree;                 // headers 的语法树
    bool missing_paramset = false;
};

class StreamSession {
public:
    bool open(const std::string& path);     // .h264 / .mp4 自动识别
    const std::vector<FrameRecord>& frames() const;
    const ParameterSetStore& parameterSets() const;
    const uint8_t* esData() const;          // 给 Hex 视图用
    size_t esSize() const;

    using ProgressCb = std::function<void(int percent)>;
    void setProgressCallback(ProgressCb);
};
```

**`open()` 在 MVP 阶段是同步阻塞**(配合进度回调),由 GUI 层放到 worker thread。不支持中途取消。

### 3.3 Decoder Core (`libstreameye_decoder.a`)

```cpp
class FFmpegDecoder {
public:
    bool init(const ParameterSetStore& psets);
    bool decodeFrame(int frame_index, DecodedFrame& out);  // 随机访问
    bool decodeNext(DecodedFrame& out);                    // 顺序访问
};

struct DecodedFrame {
    int width, height;
    std::vector<uint8_t> y_plane, u_plane, v_plane;
    int y_stride, uv_stride;
    int source_frame_index;
};
```

**随机访问策略**:从最近 IDR 重启解码,逐帧推进到目标。MVP 简化做法,性能不极致。
**YUV → QImage 转换**放在 GUI 层(用 swscale 或手写转换),Decoder 不依赖 Qt。

### 3.4 GUI (`streameye_app`)

| 类 | 角色 |
|---|---|
| `MainWindow` | 持有 `StreamSession`,组织所有 Dock 面板 |
| `FrameListPanel` | `QTableView`,展示 Frame# / POC / Type / Size |
| `SyntaxTreePanel` | `QTreeView`,Frame 切换时刷新 |
| `HexViewerPanel` | `QAbstractScrollArea` 派生,按需绘制,接收 SyntaxNode 选中信号做高亮 |
| `PreviewPanel` | `QGraphicsView`,异步从 Decoder 拿 YUV → QImage 显示 |

### 模块依赖图

```
streameye_app  ──▶  streameye_model  ──▶  streameye_parser
                          │
                          └──▶  streameye_decoder  ──▶  FFmpeg
```

---

## 4. UI 布局(默认 — Layout A · 四宫格 IDE 风)

```
┌────────────────────────────────────────────────────────────────┐
│ 📁 Open    ▶ Decode    ⚙ Settings                              │  ← Toolbar
├──────────────┬─────────────────────────────────┬───────────────┤
│ FRAME LIST   │  PREVIEW                        │ SYNTAX TREE   │
│              │                                 │               │
│ #0 IDR 12KB  │  ┌───────────────────────────┐  │ ▼ NAL [SPS]   │
│ #1 P    3KB  │  │                           │  │   profile:66  │
│ #2 B    1KB  │  │   解码后画面 (YUV→QImage)  │  │   level:31    │
│ #3 P    4KB  │  │                           │  │ ▼ NAL [Slice] │
│ #4 B    1KB  │  └───────────────────────────┘  │   type: P     │
│              ├─────────────────────────────────┤   frame_num:1 │
│              │ HEX VIEWER                      │               │
│              │ 0000: 00 00 00 01 67 42 c0 1f  │               │
│              │ 0008: d9 00 [8b 4b] 20 00 00 03│               │
│              │ 0010: 00 80 00 00 1e 07 8a 14  │               │
└──────────────┴─────────────────────────────────┴───────────────┘
```

- 全部面板用 Qt `QDockWidget`,运行时可拖拽重组
- 这里定义的是**首次启动的默认布局**

---

## 5. 关键数据流

### 流程 A — 打开文件

```
User: File→Open
   ↓
MainWindow::openFile(path)
   ↓
[worker thread] StreamSession::open(path)
   ├─ FileSource: 检测扩展名 + 魔数
   ├─ if MP4/TS: FFmpeg avformat 解封 → Annex-B ES buffer
   │  if ES:    mmap 整个文件
   ├─ NALSplitter::split(buffer) → vector<NALUnit>
   └─ for each NAL:
        - SPS/PPS → ParameterSetStore + 语法树
        - SliceHeader → FrameRecord(POC, type, frame_num)
        - 进度回调
   ↓
emit sessionReady() → 主线程
   ↓
FrameListPanel 刷新
```

### 流程 B — 选中帧

```
FrameListPanel::selectionChanged(N)
   ├─ SyntaxTreePanel: 读 frames[N].syntax_tree → 刷新
   ├─ HexViewerPanel: scrollTo(frames[N].byte_offset_in_es) + 整帧高亮
   └─ PreviewPanel: 异步派活
        [decode thread] FFmpegDecoder::decodeFrame(N)
           ↓ YUV buffer
        主线程 → QImage 转换 → 显示
```

### 流程 C — 语法字段联动 Hex

```
SyntaxTreePanel: node selected
   ↓ emit nodeSelected(SyntaxNode*)
HexViewerPanel: bit_offset/8 → 字节范围 → 滚动 + 高亮
```

---

## 6. 线程模型

| 线程 | 职责 | 生命周期 |
|---|---|---|
| 主线程(UI) | Qt event loop,所有面板绘制和用户交互 | 整个应用 |
| Parsing 线程 | 跑一次性的 `StreamSession::open()`,完成后退出 | 单次任务 |
| Decoding 线程 | 接收主线程派的解码请求,异步返回 YUV | 长驻 |

线程间通信:Qt `QMetaObject::invokeMethod` + `Qt::QueuedConnection`。`StreamSession` 自身不依赖 Qt,跨线程调度由 `MainWindow` 负责。

---

## 7. 错误处理

| 场景 | 处理 |
|---|---|
| 文件打不开 / 格式不识别 | `StreamSession::open()` 返回 false → UI 弹一次性 message box + 状态栏显示错误 |
| 截断 NAL | Parser 解到哪算哪,SyntaxNode `incomplete=true` → UI 黄色显示 |
| SPS/PPS 缺失但已遇 Slice | FrameRecord `missing_paramset=true` → 面板正常展示能解析的字段,该帧 decode 失败 |
| 单帧解码失败 | PreviewPanel 显示占位图(decode failed at frame #N),其他帧不受影响 |
| Parser bug / 字段越界 | Debug:`assert` fail-fast;Release:吞掉并记录日志 |
| 大文件(>1GB) | mmap;进度回调显示进度条;**MVP 不支持取消** |
| FFmpeg 链接缺失 | CMake 时硬失败,**不做运行时降级** |

**所有错误用返回值 + 状态字段表达,严禁使用 C++ 异常**(跨 ABI 不友好 + 嵌入式风格习惯)。

---

## 8. 项目结构

```
elecard_eye_mac/
├── CMakeLists.txt                # 顶层
├── cmake/
│   ├── FindFFmpeg.cmake
│   └── EnableTesting.cmake
├── libs/
│   ├── parser/                   # libstreameye_parser
│   │   ├── include/parser/...
│   │   ├── src/
│   │   │   ├── bit_reader.cpp
│   │   │   ├── nal_splitter.cpp
│   │   │   └── h264_syntax_parser.cpp
│   │   └── tests/
│   ├── model/                    # libstreameye_model
│   ├── decoder/                  # libstreameye_decoder
│   └── io/                       # libstreameye_io
├── app/                          # streameye_app
│   ├── main.cpp
│   ├── main_window.cpp/.h
│   ├── panels/
│   │   ├── frame_list_panel.cpp
│   │   ├── syntax_tree_panel.cpp
│   │   ├── hex_viewer_panel.cpp
│   │   └── preview_panel.cpp
│   └── tests/
├── tests/
│   └── fixtures/                 # 测试数据(CMake 生成,不入仓)
└── docs/
    └── superpowers/specs/
```

---

## 9. 构建与依赖

```cmake
cmake_minimum_required(VERSION 3.20)
project(elecard_eye_mac CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(APPLE)
  add_compile_options(-Wno-deprecated-declarations)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                  libavformat libavcodec libavutil libswscale)

include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(libs/parser)
add_subdirectory(libs/model)
add_subdirectory(libs/decoder)
add_subdirectory(libs/io)
add_subdirectory(app)
```

| 依赖 | 版本 | 安装方式 |
|---|---|---|
| Qt | 6.5+ | `brew install qt@6` |
| FFmpeg | 6.0+ | `brew install ffmpeg`(开发期),Release 改静态链接 |
| GoogleTest | 1.14 | `FetchContent`,无需预装 |
| CMake | 3.20+ | `brew install cmake` |

---

## 10. 测试策略

```
        ┌─────────────────────┐
        │  UI Smoke Tests     │   ~10 个,QTest
        └─────────────────────┘
       ┌─────────────────────────┐
       │  Integration Tests       │  ~30 个,GoogleTest
       └─────────────────────────┘
   ┌──────────────────────────────────┐
   │      Unit Tests                   │  ~100+,GoogleTest
   └──────────────────────────────────┘
```

| 模块 | 测试类型 | 重点 |
|---|---|---|
| Parser Core | 单元 | 每个语法字段:塞入手工 RBSP 字节,断言解析结果;`BitReader` 边界(跨字节、对齐) |
| NALSplitter | 单元 | 3/4 字节起始码、EP3 escape、文件尾不完整 NAL |
| Stream Model | 单元 + 集成 | FrameRecord 填充、POC 计算 |
| Decoder | 集成 | 喂已知 SHA256 的 .h264 → 解码第 N 帧 → YUV 平面 SHA256 比对 |
| GUI | 烟雾 | QTest 触发"打开文件" → 断言 FrameListPanel 行数 > 0 |

### 测试数据

- **不入仓**:用 ffmpeg 现场生成的 `.h264`/`.mp4` 文件,CMake 配置阶段生成到 `build/fixtures/`
- **入仓**:少量手工构造的字节级 fixture(C++ `static constexpr uint8_t kSPS[] = {...}`)

最小集:
- `01_idr_only.h264` — 5 帧 IDR,Baseline,128×128
- `02_ipbb.h264` — 8 帧 IPBBPBBI,Main,Open GOP
- `03_with_b_refs.h264` — 16 帧带 B 参考,High Profile
- `04_short.mp4` — 上述 ES 套 MP4 封装

### CI(MVP 之后)

- `tests/run-all.sh` 一键全测
- 每个 PR:`cmake --build && ctest`
- macOS 上 ASan/UBSan 跑 Debug 单元测试

---

## 11. 后续扩展(MVP 之后的演进路径)


设计层面 MVP 已经为以下扩展留好接口,**MVP 不实现**,但写代码时不能堵死这些路径:

1. **H.265/AV1/VVC 支持** — 各自实现 `ISyntaxParser` 接口,`StreamSession` 按 codec 选择
2. **宏块/CU 层可视化** — `H264SyntaxParser` 加 `parseSliceData()`;`PreviewPanel` 加 overlay 层(MB 边界、MV 箭头、QP 色块)
3. **GOP 时间轴** — 新增 `TimelinePanel`,使用现有 `FrameRecord` 数据
4. **CSV 导出** — Stream Model 增加 export 方法
5. **质量指标** — 新增 `libstreameye_metrics`,调 ffmpeg 的 libavfilter PSNR/SSIM
6. **跨平台** — Qt + 自研 parser 已是跨平台;Decoder 唯一平台差异是 mmap(Windows 用 `MapViewOfFile`)

---

## 12. 开发实践 — DDD + TDD

本项目在实施阶段必须遵循两条方法论硬约束。

### 12.1 DDD(领域驱动设计)

**Ubiquitous Language**:代码命名、文档、commit message 全部使用 H.264/H.265 规范术语。
- ✅ `parseSliceHeader`, `ParameterSetStore`, `nal_unit_type`, `poc_lsb`
- ❌ `parseHeader`, `ConfigCache`, `frameType`(脱离规范的自造词)

**分层与依赖方向**(domain 永远在中心,infra/UI 围绕它):

```
┌───────────────────────────────────────────────────────────┐
│  Presentation     app/  (Qt 6)                             │
└────────────────────────────────┬──────────────────────────┘
                                 ▼
┌───────────────────────────────────────────────────────────┐
│  Application      StreamSession (aggregate root,协调用例)  │
└────────────────────────────────┬──────────────────────────┘
                                 ▼
┌───────────────────────────────────────────────────────────┐
│  Domain           libs/model + libs/parser                 │
│                   纯 C++17,无 Qt/FFmpeg/OS 依赖              │
│                   FrameRecord, SyntaxNode, NALUnit,         │
│                   ParameterSetStore, H264SyntaxParser       │
└───────────────────────────────────────────────────────────┘
                                 ▲
                                 │ ports (interfaces)
                                 │
┌────────────────────────────────┴──────────────────────────┐
│  Infrastructure   libs/decoder (FFmpeg) + libs/io (mmap)   │
└───────────────────────────────────────────────────────────┘
```

- **Aggregate Root**:`StreamSession`,所有 `FrameRecord` 必须通过它访问
- **Entity**:`FrameRecord`(有 identity:`frame.index`)
- **Value Object**:`SyntaxNode`、`NALUnit`、`DecodedFrame`(纯数据,无 identity)
- **Domain Service**:`H264SyntaxParser`(把字节流变成 domain 对象的无状态服务)
- **禁止泄漏**:FFmpeg 的 `AVFrame*` / `AVCodecContext*` **不允许出现在 domain 层签名里**;decoder 输出 `DecodedFrame`(纯 std 类型)再交付

### 12.2 TDD(测试驱动开发)

**铁律**:任何新功能、字段、bug 修复,**先写失败的测试,再写实现**。流程:

```
红 (failing test) ──▶ 绿 (minimal impl) ──▶ refactor ──▶ 提交
```

具体到本项目:

| 场景 | TDD 落地方式 |
|---|---|
| Parser 新增字段(如 `level_idc`) | 在 `libs/parser/tests/h264_sps_test.cpp` 加一条:塞入手工 SPS 字节 → 断言 `node["level_idc"].value == "31"`。先看到红,再去 parser 加解析 |
| NALSplitter 新增 corner case | 在 `libs/parser/tests/nal_splitter_test.cpp` 加 fixture(如双重 EP3)→ 断言切割结果。先红再绿 |
| Bug 修复 | 先复现这个 bug 写一个测试,看到它红,再去修代码,绿后提交 |
| GUI 新面板 | 用 QTest 写最小烟雾测试(创建面板 → 喂 mock model → 断言行数/可见性) |

**禁止**:
- ❌ 先写实现再补测试
- ❌ "测试以后再写"
- ❌ 提交时 ctest 跳过任何 test
- ❌ 用 `DISABLED_` 前缀绕开 failing test(除非有 issue 链接说明)

**CMake 集成**:
- `enable_testing()` + `add_test()`,`ctest` 一键全测
- Debug 构建默认开 ASan + UBSan,跑测试自动验内存安全
- 推荐(MVP 后)CI 上要求 `ctest --output-on-failure` 必须全绿才能 merge

---

## 13. 风险与未决项

| 风险 | 缓解 |
|---|---|
| FFmpeg 私有头风险 — 万一需要内部信息 | 通过 `avcodec_parameters_*` 公开 API 拿 SPS/PPS,**Parser 不依赖 FFmpeg** 故可完全自给 |
| Qt 6 在 Apple Silicon 上的二进制可用性 | brew 已有 universal2 build;若问题严重转 source build |
| 大文件 mmap 内存占用 | mmap 是虚拟映射,实际占用由 OS 调页,问题不大 |
| GoogleTest 与 Qt MOC 在同一 target 共存 | 测试 target 独立于 GUI target,避免混合 |

**未决项**:无。所有 MVP 范围决策已在本文档中明确。
