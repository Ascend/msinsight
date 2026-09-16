# MindStudio Insight 案例集

在MindStudio Insight使用中，我们将汇总在内存分析以及性能调优等场景下的典型应用案例，帮助开发者快速掌握工具的核心功能和使用技巧。通过提供可复现的分析路径和最佳实践，助力用户高效定位在推理以及训练过程中的性能瓶颈与资源异常问题。

|使用案例|场景说明|
|-|-|
|《[基于 MindStudio Insight 定位集群快慢卡问题](./Cluster_Slow_Rank_Analysis.md)》|以“Summary 发现卡间性能差异 → Communication 定位等待/同步异常 → Timeline 对比快慢卡 → 判断慢卡负载偏高”为主线，介绍使用 MindStudio Insight 定位集群快慢卡问题的分析方法。|
|《[基于 Linux Kernel ftrace 的 Host Bound 问题分析](./Host_Bound_Analysis_with_Linux_Kernel_Trace.md)》|分析 Host Bound 问题时，通常需要采集 Linux Kernel ftrace 数据来观察 CPU 上的进程调度情况。MindStudio Insight 提供了 ftrace_tools，其中 trace_record.py 用于采集 ftrace 数据，trace_convert.py 用于转换数据格式，trace_analyze.py 用于离线统计分析。这些脚本支持将 ftrace 数据与 Profiling 数据导入同一工程进行联合分析。|
|《[JupyterLab安装指南](./Jupyter_Plugin_Installation_Guide.md)》|对于大模型的性能测试来说，往往采集的Profiling很大，或存在无法从服务器导出的场景。这时候使用图形化界面分析存在困难。MindStudio Insight 提供了基于 Jupyter 的插件可供使用，无需从服务器下载 Profiling 数据到本地，可以通过 Jupyter 以 web 界面的形式在本地进行分析。|
|《[基于 MindStudio Insight 分析 Cube-Vector 融合算子性能](./Operator_Fusion_Analysis.md)》|本案例以 Ascend C 官方示例 [MatmulLeakyRelu 融合算子](https://gitcode.com/cann/asc-devkit/tree/master/examples/01_simd_cpp_api/00_introduction/03_fusion_operation) 为例，介绍如何使用 MindStudio Insight 分析 Cube-Vector 融合算子的性能。|
|《[基于 PyTorch Snapshot 数据分析内存问题案例](./profiler_memory_example.md)》|在强化学习等复杂训练场景中，仅靠整体内存曲线难以区分内存泄漏、瞬时峰值或碎片问题。本案例通过导入PyTorch Snapshot数据到MindStudio Insight，介绍如何利用内存块生命周期图、内存池状态图及调用栈等信息，精准定位内存异常的具体阶段、来源及碎片情况。|
|《[基于 PyTorch Snapshot 定位 ResNet50 训练内存泄漏](./pytorch_snapshot_memory_analysis.md)》|本案例通过构建ResNet50训练场景，演示了因将Device Tensor直接存入历史列表导致Python引用无法释放、进而引发显存泄漏甚至OOM的典型问题及修复方案。文本结合Pytorch Snapshot分析与长周期训练验证，证明了仅记录Host标量值可有效避免此类内存泄漏。|
|《[单卡 Top-Down 算子下发瓶颈分析](./Single_Card_Top_Down_Dispatch_Analysis.md)》|本案例以下发瓶颈为例，介绍如何从 Overlap Analysis 的总体占比出发，逐步定位到算子下发链路和可能的 Host 侧根因。|
|《[verl场景下Snapshot数据采集和分析案例](./verl_Memory_Snapshot_Collection_and_Analysis.md)》|本文介绍了在Verl框架种利用内置`torch_memory`采集PPO等训练阶段显存快照的方法，以定位峰值来源、监控显存泄露及对比Rank间差异。同时，文章提供了基于MindStudio Insight对采集数据进行可视化分析的具体案例。|
|《[Timeline泳道介绍](./Timeline_Common_Lanes_and_Interface.md)》|1. Timeline(时间线)常用泳道与界面有哪些？彼此之间有什么关系？<br>2. 什么是Overlap Analysis（覆盖分析）？<br>3. Timeline常用于观察哪些问题？|
|《[快捷键说明](./Keyboard_Shortcuts.md)》|在 MindStudio Insight 中，我们致力于为用户提供更流畅、更高效的操作体验。为了帮助大家更快速地掌握这些高效工具，本文将介绍一些常用的键盘快捷键，助力您快速上手，提升操作效率。|
