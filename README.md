# Elecard Eye Mac

H.264 码流分析器(macOS / Qt 6),对标 Windows 端 Elecard StreamEye。**学习项目,练手 codec 解析内核 + macOS 工程**。

当前阶段:**MVP — 仅 H.264**。规划逐步扩到 H.265 / AV1 / VVC。详见 [设计文档](docs/superpowers/specs/2026-05-27-elecard-eye-mac-design.md)。

## 依赖

| 工具 | 版本 | 用途 |
|---|---|---|
| Homebrew | latest | 包管理 |
| Qt | 6.5+(实测 6.11.1) | GUI |
| FFmpeg | 6.0+(实测 62.x) | 解码 + MP4 解封 |
| CMake | 3.23+ | 构建(需要支持 CMakePresets v4) |
| pkg-config | — | FFmpeg 探测 |

平台:macOS 13+(Apple Silicon 优先,Intel 应该也能跑但未验证)。

## 快速开始

```bash
# 一键装依赖 + configure(可重复跑)
./scripts/setup.sh

# 编译
cmake --build build

# 测试
ctest --test-dir build -V
```

如果不想用 `setup.sh`,手工三步:

```bash
brew install qt cmake ffmpeg pkg-config
cmake --preset default
cmake --build build && ctest --test-dir build -V
```

## 项目结构

```
elecard_eye_mac/
├── libs/
│   ├── io/         # FileSource(mmap)
│   ├── parser/     # 自研:BitReader / NALSplitter / SPS·PPS·SliceHeader parser
│   ├── model/      # Domain: StreamSession (aggregate root), FrameRecord, POC
│   └── decoder/    # FFmpeg 封装(infrastructure)
├── app/            # Qt 主程序 + 4 个 Dock 面板(Phase D+)
├── tools/
│   └── dump_frames/  # 无 GUI 的 CLI 演示工具
├── tests/
│   └── gen_fixtures.sh   # ffmpeg 生成 4 个测试 .h264/.mp4
├── cmake/          # CMake helpers
└── docs/superpowers/
    ├── specs/      # 设计文档
    ├── plans/      # 实施计划
    └── reference/  # 每个源码文件对应的参考实现 doc
```

## 开发实践

- **DDD**:domain 层(`libs/model` + `libs/parser`)零外部依赖,FFmpeg/Qt 类型不允许泄漏进 domain
- **TDD 铁律**:测试先行,红 → 绿 → refactor → commit
- **错误处理**:返回值 + 状态字段,**禁用 C++ 异常**
- **构建**:Debug 默认开 ASan + UBSan

详见 [设计文档 §12 开发实践](docs/superpowers/specs/2026-05-27-elecard-eye-mac-design.md#12-开发实践--ddd--tdd)。

## 文档导航

- [设计文档(spec)](docs/superpowers/specs/2026-05-27-elecard-eye-mac-design.md) — 架构、组件、UI 布局、数据流、错误处理
- [实施计划(plan)](docs/superpowers/plans/2026-05-27-elecard-eye-mac-mvp.md) — Phase A→H 的可执行 Tasks
- `docs/superpowers/reference/<path>-reference.md` — 各源码文件的参考实现(实现完对照学习用)
