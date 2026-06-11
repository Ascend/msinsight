# 基于 PyTorch Snapshot 数据分析内存问题案例

## 案例背景

在训练场景中，模型在较小 batch size 下可以正常运行，但当 batch size 增大到某个阈值后却突然出现 OOM，这类问题通常不只是“显存不够”这么简单。实际分析时，除了关注整体显存峰值，还需要结合 profiling 数据判断是否存在异常的算子 workspace 申请、shape 放大、或者某个阶段的临时内存被成倍拉高。

本案例以一个 batch size 从 2 增加到 3 后触发 OOM 的问题为例，介绍如何先通过 profiling 数据观察显存占用趋势，再结合异常算子和调用栈信息定位到具体原因。

## 分析方法论

针对这类“batch size 增大后 OOM”的问题，建议按照以下顺序分析：

1. **先看整体显存峰值**：确认问题是单次峰值过高，还是某种持续累积导致的 OOM。
2. **再看静态显存占用**：区分基础占用、PTA 统计占用和算子临时申请，判断是否存在明显异常。
3. **重点检查 Reserved 与 Allocated 差值**：如果两者之间存在较大 gap，需要继续排查 workspace、碎片化或未统计到的组件占用。
4. **回到异常算子和 timeline**：通过 profiler 中的大内存申请事件，定位具体算子及其对应的 shape。
5. **结合源码确认根因**：最后检查算子前后的 shape 变化和类型转换顺序，判断是否存在不必要的大内存放大。

这类问题通常不是“模型整体太大”，而是某个中间张量在特定顺序下被放大，导致 workspace 成倍增加。

## 问题现象

某训练场景在 `bs=2` 时可以正常训练，但当 batch size 提升到 `bs=3` 后出现 OOM。按照模型规模和历史经验判断，该配置理论上应当可以承载 `bs=3`，因此需要进一步分析是否存在异常显存申请。

OOM 日志如下：

```text
File "<path-to-py-file>/transformer.py", line xx, in forward
    attn_mask = attn_mask.bool()
RuntimeError: NPU out of memory. Tried to allocate 48.78 GiB (NPU 0; 60.96 GiB total capacity; 13.22 GiB already allocated; 13.22 GiB current active; 40.54 GiB free; 54.86 GiB allowed; 13.86 GiB reserved in total by PyTorch) If reserved memory is >> allocated memory try setting max_split_size_mb to avoid fragmentation.
```

从报错信息看，OOM 发生在 `attn_mask.bool()` 这样的类型转换操作上，而且申请量达到了 48GiB 级别，明显异常。

## 信息收集

在继续分析前，先补充两个关键信息：

1. 训练任务开启了 `task_queue_enable=2`。
2. 需要对比 `bs=2` 和 `bs=3` 两种场景下的 profiling 数据。

这里之所以要特别关注 `task_queue_enable=2`，是因为该模式下 profiling 中的 PTA reserved / allocated 统计不一定包含 workspace，需要结合其它视图继续确认是否存在独立的 workspace 占用。

## Profiling 数据分析

### 先看 bs=1 和 bs=2 的基线

先从较小 batch size 的 profiling 数据入手，可以帮助确认问题不是由模型本身的基础占用持续增长引起的。

在 `bs=1` 时，profiling 中 observed 的 allocated 峰值约为 21G，其中静态显存占用大约 11G。

在 `bs=2` 时，静态显存占用仍然约为 11G，但 allocated 峰值上升到约 26G。这个结果说明模型的基础占用没有明显异常，但随着 batch size 增大，峰值显存开始快速上升。

此时在 profiling 图中还能看到一个值得注意的现象：`APP Reserved` 与 `Operator Reserved` 之间存在较大的 gap，峰值接近 20G+。这通常意味着除了已知的算子申请之外，还存在其它异常占用来源，需要继续向下追查。

### 再看 bs=3 的 OOM 对应特征

当 batch size 增加到 `bs=3` 后，训练直接 OOM，且报错点出现在 `attn_mask.bool()` 这一类 cast 操作上。由于该位置本身不是大模型参数加载或长期缓存路径，而是一个中间张量类型转换点，因此更像是一次临时 workspace 申请失败。

结合 `bs=2` 的 profiling 结果和 `bs=3` 的 OOM 日志，可以初步判断：

- 问题不是模型权重或静态显存本身爆掉；
- 问题也不像是 CANN 组件独占了大量显存；
- 更可能是某个算子在特定 shape 下申请了极大的 workspace。

### 排除非 PTA 占用

为了确认异常占用是否来自 PTA 之外的组件，继续查看 `npu_module_mem`。结果显示，CANN 组件占用不到 4G，整体并不异常。

接着查看算子申请表，也没有发现明显“未统计进 PTA”的大额占用。因此可以进一步缩小范围：问题更可能出在某个算子的临时申请，而不是外部组件长期占用。

### 关注 Workspace 统计是否缺失

这里还有一个关键线索：当 `task_queue_enable=2` 时，profiling 中的 PTA reserved / allocated 不一定覆盖所有 workspace 行为，而应该有单独的 workspace 统计项。若图中看不到明显的 Workspace 占用，就需要怀疑某个算子内部发生了非常大的临时申请，但没有在常规视图中直观体现出来。

这一步的意义在于：不要只盯着整体 reserved/allocated 数字，还要结合具体算子和调用栈去找“谁在申请这块内存”。

### 通过算子排序和 timeline 定位异常申请

为了进一步确认问题，先将 `task_queue_enable` 调整为 1，并重新采集 `bs=2` 的 profiling 数据。这样可以更清楚地观察单个算子申请和峰值尖刺。

在缩放后的 profiling 图中，按算子申请大小从大到小排序后，可以发现一个异常的 `cast` 算子，其总内存申请达到 32.5G。随后跳转到 timeline 查看对应 shape，可以发现该申请量与输入 shape 高度相关。

根据计算公式：

```text
2 * 24 * 9536 * 9536 * 8 / 1024 / 1024 / 1024 = 32.5G
```

当 batch size 提升到 3 时，这个值会进一步上升到约 48G，与 OOM 日志中的申请量基本一致。

这说明问题并不是随机波动，而是某个中间张量的 shape 在进入 cast 时已经被放大到了极大规模。

## 分析结论

结合 profiling 数据和 OOM 日志可以得到结论：前向过程中，`attention mask` 先执行了 `expand`，将 shape 扩大到一个非常大的维度，然后再执行 `bool()` 转换，导致 cast 阶段申请了巨大的 workspace。随着 batch size 增大，这个 workspace 按 shape 线性膨胀，最终在 `bs=3` 时触发 OOM。

换句话说，问题的本质不是模型参数过大，而是**中间张量在错误的顺序下被放大**，使得一个本应较轻量的类型转换操作变成了超大内存申请。

## 修复建议

修复思路很直接：调整操作顺序，先执行 `bool()`，再执行 `expand()`。

原始逻辑：

```python
attn_mask = attn_mask.expand(batch_size, self.num_attention_heads, total_seq_len, -1)
attn_mask = attn_mask.bool()
```

建议调整为：

```python
attn_mask = attn_mask.bool()
attn_mask = attn_mask.expand(batch_size, self.num_attention_heads, total_seq_len, -1)
```

这样可以避免在超大 shape 上执行 cast，从而显著降低 workspace 占用。

## 小结

这类问题的定位关键在于：先通过 profiling 识别峰值异常，再结合算子排序和 timeline 找到大内存申请发生的位置，最后回到源码检查 shape 放大顺序。对于 batch size 增大后突然 OOM 的场景，优先排查中间张量是否在类型转换、reshape、expand、broadcast 等操作前被放大，往往能更快定位根因。
