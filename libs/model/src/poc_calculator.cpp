#include "model/poc_calculator.h"

namespace model {

POCCalculator::POCCalculator(int log2_max_pic_order_cnt_lsb)
    : max_poc_lsb_(1 << log2_max_pic_order_cnt_lsb) {}

int POCCalculator::compute(bool is_idr, int pic_order_cnt_lsb) {
    // TODO(C.1 green):实现 §8.2.1.1 的 POC type 0 算法。
    //
    // 1) IDR 重置:
    //      if (is_idr) { prev_msb_ = 0; prev_lsb_ = 0; }
    //
    // 2) 算 msb(wrap-around 判定,half = max_poc_lsb_ / 2):
    //      int msb;
    //      if (pic_order_cnt_lsb < prev_lsb_ &&
    //          (prev_lsb_ - pic_order_cnt_lsb) >= half)
    //          msb = prev_msb_ + max_poc_lsb_;
    //      else if (pic_order_cnt_lsb > prev_lsb_ &&
    //               (pic_order_cnt_lsb - prev_lsb_) > half)
    //          msb = prev_msb_ - max_poc_lsb_;
    //      else
    //          msb = prev_msb_;
    //
    // 3) poc = msb + pic_order_cnt_lsb;
    //
    // 4) 推进状态(MVP 每帧都更新):
    //      prev_msb_ = msb; prev_lsb_ = pic_order_cnt_lsb;
    //
    // 5) return poc;
    //
    // 注意:IDR 那帧 prev 先归零再走 2)→3),结果 poc=0(lsb 通常也是 0)。
    (void)is_idr;
    (void)pic_order_cnt_lsb;
    (void)max_poc_lsb_;  // 实现时删除这三行
    (void)prev_msb_;
    (void)prev_lsb_;
    return 0;
}

}  // namespace model
