#include "parser/parameter_set_store.h"

namespace parser {

//
// addSPS(id, tree):   sps_[id] = std::move(tree);
// addPPS(id, tree):   pps_[id] = std::move(tree);
// findSPS(id) const:  auto it = sps_.find(id);
//                     return it == sps_.end() ? nullptr : &it->second;
// findPPS(id) const:  同上,查 pps_。
// clear():            sps_.clear(); pps_.clear();
//
// 注意:findXxx 返回 const SyntaxNode*,指向 map 内部元素,不要返回局部拷贝。

void ParameterSetStore::addSPS(int id, SyntaxNode tree) {
    sps_[id] = std::move(tree);
}

void ParameterSetStore::addPPS(int id, SyntaxNode tree) {
    pps_[id] = std::move(tree);
}

const SyntaxNode* ParameterSetStore::findSPS(int id) const {
    auto it = sps_.find(id);
    return it == sps_.end() ? nullptr : &it->second;
}

const SyntaxNode* ParameterSetStore::findPPS(int id) const {
    auto it = pps_.find(id);
    return it == pps_.end() ? nullptr : &it->second;
}

void ParameterSetStore::clear() {
    sps_.clear();
    pps_.clear();
}

}  // namespace parser
