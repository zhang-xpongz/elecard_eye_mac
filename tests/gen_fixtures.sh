#!/usr/bin/env bash
set -euo pipefail
OUT="${1:?usage: gen_fixtures.sh <out_dir>}"
mkdir -p "$OUT"

# 01: 5 IDR-only frames, Baseline Profile, 128x128
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=5:duration=1" \
  -c:v libx264 -profile:v baseline -g 1 -bf 0 -pix_fmt yuv420p \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/01_idr_only.h264"

# 02: 8 frames IPBBPBBI, Main Profile, Open GOP
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=8:duration=1" \
  -c:v libx264 -profile:v main -g 4 -bf 2 -pix_fmt yuv420p \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/02_ipbb.h264"

# 03: 16 frames with B refs, High Profile
ffmpeg -y -f lavfi -i "testsrc=size=128x128:rate=16:duration=1" \
  -c:v libx264 -profile:v high -g 8 -bf 3 -refs 4 -pix_fmt yuv420p \
  -bsf:v h264_mp4toannexb -f h264 "$OUT/03_with_b_refs.h264"

# 04: mp4 container wrapping 02
ffmpeg -y -i "$OUT/02_ipbb.h264" -c copy -f mp4 "$OUT/04_short.mp4"

echo "Fixtures generated in $OUT"
