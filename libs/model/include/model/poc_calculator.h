#pragma once

namespace model {

// =============================================================================
// POCCalculator — Picture Order Count 计算器(H.264 §8.2.1.1,仅 POC type 0)
// =============================================================================
//
// POC 决定帧的**显示顺序**(与解码顺序不同,B 帧会乱序)。MVP 只实现
// pic_order_cnt_type==0 的算法:解码器只传 pic_order_cnt_lsb(低位),高位
// (MSB)由本类根据前一帧的 lsb 推断 wrap-around 累加得到。
//
// 用法:每个 GOP/序列一个实例(用 SPS 的 log2_max_pic_order_cnt_lsb 构造),
// 按**解码顺序**逐帧调 compute()。遇到 IDR 会重置内部状态。
//
// 状态机:持有 prevPicOrderCntMsb / prevPicOrderCntLsb。MVP 简化——每帧都把
// prev 更新为当前帧(spec 严格版只在参考帧且无 MMCO=5 时更新,MVP 不区分)。
//
// 不抛异常。POC type 1/2 不在本类职责内(StreamSession 对那些帧记 poc=0)。
//
class POCCalculator {
public:
    // -------------------------------------------------------------------------
    // 构造
    //
    // log2_max_pic_order_cnt_lsb  = SPS 的 log2_max_pic_order_cnt_lsb_minus4 + 4。
    //   MaxPicOrderCntLsb = 2 ^ 该值(如传 4 → 16)。决定 lsb 回绕周期。
    explicit POCCalculator(int log2_max_pic_order_cnt_lsb);

    // -------------------------------------------------------------------------
    // compute — 给一帧算 POC,并推进内部状态
    //
    // 参数:
    //   is_idr              该帧是否 IDR。IDR 会先把 prevMsb/prevLsb 归零。
    //   pic_order_cnt_lsb   从该帧 slice header 解出的 pic_order_cnt_lsb。
    //
    // 返回:该帧的 PicOrderCnt(= PicOrderCntMsb + pic_order_cnt_lsb)。
    //
    // 算法(§8.2.1.1):
    //   half = MaxPicOrderCntLsb / 2
    //   若 lsb < prevLsb 且 (prevLsb - lsb) >= half → msb = prevMsb + Max
    //   否则若 lsb > prevLsb 且 (lsb - prevLsb) >  half → msb = prevMsb - Max
    //   否则 → msb = prevMsb
    //   poc = msb + lsb;然后 prevMsb=msb, prevLsb=lsb
    //
    // 副作用:更新 prevMsb / prevLsb(IDR 时先归零再算)。
    int compute(bool is_idr, int pic_order_cnt_lsb);

private:
    int max_poc_lsb_;      // MaxPicOrderCntLsb = 2 ^ log2_max_pic_order_cnt_lsb
    int prev_msb_ = 0;
    int prev_lsb_ = 0;
};

}  // namespace model
