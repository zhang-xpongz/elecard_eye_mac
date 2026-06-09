# `parser::ParameterSetStore` — 参考实现

> ⚠️ 自己实现完、5 个测试全绿后再看。

---

## 1. 职责

按 id 存取已解析的 SPS / PPS 语法树。两个独立的 `unordered_map<int, SyntaxNode>`。
没有解析逻辑,纯容器封装。存在的意义是给 B.6b 的 slice header 解析提供上下文回查
(slice → pic_parameter_set_id → PPS → seq_parameter_set_id → SPS)。

## 2. 完整参考实现

```cpp
#include "parser/parameter_set_store.h"

namespace parser {

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
```

## 3. 要点

1. **`std::move(tree)`**:参数按值传入,移动进 map 避免一次深拷贝(SyntaxNode 含
   string + vector,拷贝不便宜)。`#include <utility>` 通常已被标准库间接引入,
   保险起见可显式加。

2. **`operator[]` 自带覆盖语义**:同 id 再 add 直接覆盖,正好满足"参数集流中途
   重定义后者生效"。不需要先 erase。

3. **find 返回内部指针**:`&it->second` 指向 map 元素,只要不再修改 map 就有效。
   绝不能返回局部变量地址。指针失效时机:对同一 store 再 add(可能 rehash)或
   clear 之后。调用方(slice 解析)用完即弃,不存指针。

4. **SPS / PPS 命名空间独立**:两个 map 分开,`findSPS(2)` 和 `findPPS(2)` 互不影响。

## 4. 为什么放 parser:: 而不是 model::

plan §B.6 的 DDD 决策:它存 `parser::SyntaxNode`,概念上属 parser 层的"已解析头部
仓储"。放 parser:: 让 parser 子层自洽、可独立测试,不反向依赖 model。model /
decoder 后续直接 `#include "parser/parameter_set_store.h"` 复用,不再重复定义。
与 spec §3.2 把它列在 Stream Model 表格有偏差,差异已在 plan 注明。见
[[feedback-ddd-tdd]]。
