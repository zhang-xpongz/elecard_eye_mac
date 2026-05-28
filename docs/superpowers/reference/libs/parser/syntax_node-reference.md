# `parser::SyntaxNode` — 设计说明

> 这是纯 POD,没有"实现"可言。本文档解释设计取舍,不需要你写代码。

---

## 1. 为什么用通用结构而不是专门 struct

两种设计:

### 方案 X(没采用):每个语法元素一个专门 struct

```cpp
struct SPS { int profile_idc; int level_idc; ... };
struct PPS { int pps_id; bool entropy_coding_mode; ... };
struct SliceHeader { ... };
```

优点:类型安全,字段直接成员访问。
缺点:
- UI 要为每种 struct 写专门渲染代码,加 codec 就得改 UI
- 没法统一表达"任意字段的 bit 偏移",Hex 联动难做
- H.265/AV1 的字段集完全不同,组合爆炸

### 方案 ✅(采用):通用 SyntaxNode 树

```cpp
struct SyntaxNode {
    std::string name;       // "profile_idc"
    std::string value;      // "66"
    size_t bit_offset;      // 用于 Hex 高亮
    size_t bit_length;
    bool incomplete;
    std::vector<SyntaxNode> children;
};
```

优点:
- UI 只认 name/value/children,一套渲染代码吃所有 codec
- bit_offset/bit_length 让任意字段都能在 Hex viewer 精确高亮
- 加 H.265/AV1 只是 parser 多吐节点,UI 零改动

代价:程序化取值要按 name 查找。各 parser 测试里都有个 `find()` helper 做深度优先查找。MVP 阶段这个代价可接受。

## 2. 字段约定(parser 实现时要遵守)

| 场景 | name | value | bit_offset / bit_length |
|---|---|---|---|
| 叶子字段 | 规范原名 `"level_idc"` | 渲染值 `"31"` | 该字段精确 bit 区间 |
| 枚举字段 | `"slice_type"` | `"7 (I slice)"`(带可读解释) | 字段 bit 区间 |
| 容器节点 | 结构名 `"seq_parameter_set_rbsp"` | 空 | 可设为子节点总区间或 0 |

**bit_offset 是全局偏移**:parser 解某个 NAL 时,要把"NAL 在 ES 中的基准 bit 偏移"加到字段的 NAL 内偏移上,这样 Hex viewer 才能直接定位。

## 3. 与 incomplete 标志的配合

解析中途遇到 buffer 越界 / 非法值时,把当前节点 `incomplete = true` 并停止往下解,**不抛异常**。UI 会把这种节点显示成黄色,提示用户"这里数据可能损坏"。这是项目"宽容解析"原则的体现(见 [[feedback-no-cpp-exceptions]])。

## 4. 后续扩展

- 可加 `enum class NodeKind { Field, Container, Array }` 帮 UI 区分渲染
- 可加 `std::string description` 存字段的人类可读说明(tooltip 用)
- MVP 不做,保持结构最小。
