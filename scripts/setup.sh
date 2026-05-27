#!/usr/bin/env bash
# 一键环境部署 — 安装依赖 + 首次 configure。可重复运行(idempotent)。
set -euo pipefail

# --- 1. 检查 Homebrew ---
if ! command -v brew >/dev/null 2>&1; then
  echo "✗ Homebrew 未安装。先装:https://brew.sh"
  exit 1
fi

# --- 2. 安装依赖(已装则跳过) ---
DEPS=(qt cmake ffmpeg pkg-config)
TO_INSTALL=()
for pkg in "${DEPS[@]}"; do
  if brew list "$pkg" >/dev/null 2>&1; then
    echo "✓ $pkg 已装"
  else
    TO_INSTALL+=("$pkg")
  fi
done

if [ ${#TO_INSTALL[@]} -gt 0 ]; then
  echo "→ brew install ${TO_INSTALL[*]}"
  brew install "${TO_INSTALL[@]}"
fi

# --- 3. 首次 cmake configure(若 build/ 不存在) ---
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

if [ ! -d build ]; then
  echo "→ cmake configure(preset: default)"
  cmake --preset default
else
  echo "✓ build/ 已存在,跳过 configure"
fi

cat <<EOF

环境就绪。常用命令:

  cmake --build build           # 编译
  ctest --test-dir build -V     # 跑测试
  cmake --build build --target fixtures  # 重新生成测试码流

更换 Qt 路径或想重 configure:
  rm -rf build && cmake --preset default
EOF
