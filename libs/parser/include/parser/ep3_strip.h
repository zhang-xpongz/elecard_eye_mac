#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace parser {

// =============================================================================
// stripEmulationPrevention — EBSP → RBSP,剥离 emulation prevention 字节
// =============================================================================
//
// H.264 7.4.1:编码器在 RBSP 里遇到 `00 00 {00,01,02,03}` 这种可能被误认为
// 起始码的序列时,会在 `00 00` 后插入一个 emulation_prevention_three_byte
// (0x03),变成 `00 00 03 {00,01,02,03}`,得到 EBSP。
//
// 本函数做反向操作:扫描 `00 00 03` 序列,把那个 0x03 删掉,还原成 RBSP。
// 这是连接 NALSplitter(输出 EBSP)和 SPS/PPS/SliceHeader parser(吃 RBSP)
// 的桥梁。
//
// 注意:输入应该是单个 NAL 的 payload(已去掉起始码),不要带起始码进来。
//
// -----------------------------------------------------------------------------
// 参数:
//   data   EBSP 字节(单个 NAL payload)。允许 nullptr(此时 size 必须为 0)
//   size   字节数
//
// 返回:
//   剥离 EP3 后的 RBSP 字节(新分配的 vector)。
//   - 无 `00 00 03` → 原样拷贝
//   - data==nullptr / size==0 → 空 vector
//
// 语义细节:
//   - 检测到 `00 00 03` 时,保留两个 0x00,丢弃 0x03,从 0x03 之后继续扫描
//   - `00 00 03` 出现在末尾(03 后无字节)也照样剥成 `00 00`
//   - 剥离后的 `00 00` 不参与下一轮 `00 00 03` 的重叠匹配(从 03 之后接着扫)
//
// 副作用:无。不修改输入。
//
// 参考:H.264 spec 7.3.1 / 7.4.1
std::vector<uint8_t> stripEmulationPrevention(const uint8_t* data, size_t size);

}  // namespace parser
