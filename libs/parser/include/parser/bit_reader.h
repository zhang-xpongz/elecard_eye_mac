#pragma once

#include <cstddef>
#include <cstdint>

namespace parser {

// =============================================================================
// BitReader — MSB-first 比特流读取器,带 H.264 Exp-Golomb 解码
// =============================================================================
//
// 用于解析 H.264/H.265 等比特流中按位打包的字段。语义遵循 H.264 spec 9.1
// (无符号 Exp-Golomb)和 9.1.1(有符号 Exp-Golomb)。
//
// 错误处理:本类不抛异常。任何越界 / 非法读取会设置内部 error 标志,后续读
// 调用直接短路返回 0(错误状态具粘性)。调用方应在解析完毕后用 hasError()
// 检查整体是否成功,而非每次 readBits 后都查。
//
// 不持有数据所有权:构造时传入的 (data, size_bytes) 必须在 BitReader 生命
// 周期内保持有效。典型用法是把 RBSP 数据 mmap 到内存后用 BitReader 解析。
//
class BitReader {
public:
    // -------------------------------------------------------------------------
    // 构造 — 把一段连续内存当作比特流读
    //
    // 参数:
    //   data         比特流起始指针。允许为 nullptr,但此时 size_bytes 必须为 0
    //   size_bytes   缓冲区大小(字节)。可读 bit 总数 = size_bytes * 8
    //
    // 副作用:bit_offset_ 初始化为 0,error_ 初始化为 false。
    BitReader(const uint8_t* data, size_t size_bytes);

    // -------------------------------------------------------------------------
    // readBits — 从当前 bit cursor 读取 n 个 bit,MSB-first
    //
    // MSB-first 含义:bit-0 是 data[0] 的最高位(`(data[0] >> 7) & 1`),
    // bit-7 是 data[0] 的最低位,bit-8 是 data[1] 的最高位,以此类推。
    //
    // 参数:
    //   n   要读的 bit 数,合法范围 [1, 32]
    //
    // 返回:
    //   读出的无符号值,高位补 0(例如读 4 个 bit `1010` 返回 0x0A)
    //   错误时返回 0
    //
    // 错误条件(置 error_ = true 后返回 0):
    //   - n ∉ [1, 32]
    //   - bit_offset_ + n > size_bytes * 8(越界)
    //   - 此前调用已使 error_ = true(粘性错误,直接短路)
    //
    // 副作用:
    //   成功时 bit_offset_ += n。错误时 bit_offset_ 不变。
    uint32_t readBits(int n);

    // -------------------------------------------------------------------------
    // readUE — 读一个无符号 Exp-Golomb 编码值,ue(v),H.264 spec 9.1
    //
    // 编码格式:`<leading_zeros 个 0> 1 <leading_zeros 个 bit 的尾数 V>`
    // 解码:codeNum = (2 ^ leading_zeros) - 1 + V
    //
    // 例:
    //   '1'      → 0
    //   '010'    → 1
    //   '011'    → 2
    //   '00100'  → 3
    //   '0001000'→ 7
    //
    // 返回:
    //   解码后的非负整数;错误时返回 0
    //
    // 错误条件(置 error_ = true):
    //   - 调用 readBits 时越界
    //   - leading_zeros ≥ 32(防御损坏流死循环 / 结果超 uint32)
    //
    // 副作用:成功时按编码长度推进 bit_offset_;错误时仍可能消耗了部分 bit
    uint32_t readUE();

    // -------------------------------------------------------------------------
    // readSE — 读一个有符号 Exp-Golomb 编码值,se(v),H.264 spec 9.1.1
    //
    // 内部先解出无符号 codeNum k = readUE(),再做 zigzag 映射:
    //   k=0 → 0,  k=1 → +1,  k=2 → -1,  k=3 → +2,  k=4 → -2 ...
    //   公式:k 为奇 → +(k+1)/2;k 为偶 → -k/2
    //
    // 返回:
    //   解码后的有符号整数;错误时返回 0
    //
    // 错误条件:同 readUE(透传 readUE 的错误)
    //
    // 副作用:同 readUE
    int32_t  readSE();

    // -------------------------------------------------------------------------
    // bitOffset — 当前 bit cursor 位置(从缓冲区开头算起)
    //
    // 返回:
    //   已消耗的 bit 数。例如读了 1 个字节后返回 8。
    //   常用于:
    //     - 把当前 bit 位置记入 SyntaxNode.bit_offset 给 UI 联动 Hex 用
    //     - 调试时观察解析进度
    //
    // 副作用:无
    size_t   bitOffset() const;

    // -------------------------------------------------------------------------
    // hasError — 当前是否处于错误状态
    //
    // 一旦置错就永远为 true(粘性)。建议在一段解析逻辑结束后统一检查,
    // 而非每次 read 后都查 — 提升代码可读性。
    //
    // 返回:
    //   true  发生过越界 / 非法读取
    //   false 一切正常
    //
    // 副作用:无
    bool     hasError() const;

private:
    const uint8_t* data_;
    size_t  size_bytes_;
    size_t  bit_offset_ = 0;
    bool    error_ = false;
};

}  // namespace parser
