# **MindStudio Insight算子调优**

## 简介

MindStudio Insight提供指令流水视图、算子源码视图以及算子运行负载分析视图，直观地将运行在昇腾AI处理器上的算子的关键性能指标进行可视化呈现，帮助用户快速定位算子的软硬件性能瓶颈，提升算子性能分析的效率。

根据调优目标，您可以参考以下路径选择阅读重点：

- **判断算子瓶颈类型**：先阅读[详情（Details）](details.md#details)中的Roofline瓶颈分析，判断是计算瓶颈还是内存瓶颈。
- **分析计算瓶颈**：阅读[详情（Details）](details.md#details)中的计算工作负载分析和核间负载分析。
- **分析内存瓶颈**：阅读[详情（Details）](details.md#details)中的内存负载分析和[缓存（Cache）](cache.md#cache)。
- **定位高耗时指令**：阅读[时间线（Timeline）](timeline.md#timeline)。
- **关联指令到源码**：阅读[源码（Source）](source.md#source)。
- **分析Triton算子内存溢出**：阅读[片上内存（On-Chip Memory）](on_chip_memory.md#on-chip-memory)。
- **对比优化前后效果**：阅读[详情（Details）](details.md#details)中的“支持算子间性能对比”。

## 使用前准备

**环境准备**

1. 安装MindStudio Insight工具，具体安装步骤请参见[MindStudio Insight安装指南](../../install_guide/mindstudio_insight_install_overview.md)。
2. 安装msOpProf算子调优工具，具体安装步骤请参见[msOpProf安装指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/install_guide/msopprof_install_guide.md)。

**数据采集**

算子调优场景的数据由msOpProf工具采集生成，请根据实际需求选择采集模式：

|采集模式|适用场景|采集命令示例|生成文件|
|--|--|--|--|
|上板采集（msopprof）|需要真实硬件性能数据，分析实际运行耗时、内存带宽、Cache行为等。|`msopprof --output=./output_dir ./op_executable`|visualize_data.bin|
|仿真采集（msopprof simulator）|无真实硬件或需要指令级详细分析，如指令流水追踪、代码热点定位等。|`msopprof simulator --soc-version=Ascendxxxyy --output=./output_dir ./op_executable`|trace.json、visualize_data.bin|

> [!NOTE]
>
> - 上板采集需要真实NPU环境，仿真采集无需NPU但需指定芯片型号（`--soc-version`），可通过 `python3 -c "import acl; print(acl.get_soc_name())"` 获取。
> - 两种模式互补，建议结合使用：上板采集获取真实性能数据，仿真采集获取指令级详细数据。
> - 更多采集参数和用法请参见[msopprof模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_user_guide.md)和[msopprof simulator模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_simulator_user_guide.md)。

**数据导入**

采集完成后，将生成的性能数据文件导入MindStudio Insight工具，具体数据说明请参见[数据说明](#数据说明)，数据导入操作请参见[导入数据](../basic_operations.md#导入数据)。

## 数据说明<a id="数据说明"></a>

**数据文件**

算子调优场景支持导入的性能数据文件请参见[**表 1**  支持导入的性能数据](#支持导入的性能数据)。

**表 1**  支持导入的性能数据<a id="支持导入的性能数据"></a>

|显示界面|文件名|说明|文件获取方式|
|--|--|--|--|
|时间线（Timeline）|trace.json|算子仿真指令流水图文件。|仅仿真模式。参见[msopprof simulator模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_simulator_user_guide.md)。|
|时间线（Timeline）|visualize_data.bin|指令流水图可视化呈现文件。|上板+仿真模式。参见[msopprof模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_user_guide.md)和[msopprof simulator模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_simulator_user_guide.md)。|
|详情（Details）|visualize_data.bin|算子基础信息、计算单元负载和Roofline瓶颈分析等信息的可视化呈现文件。|仅上板模式。参见[msopprof模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_user_guide.md)。|
|源码（Source）|visualize_data.bin|仿真热点函数等信息可视化呈现文件。|上板+仿真模式。参见[msopprof模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_user_guide.md)和[msopprof simulator模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_simulator_user_guide.md)。|
|缓存（Cache）|visualize_data.bin|用户程序Kernel函数内的L2 Cache访问信息可视化呈现文件。|仅上板模式。参见[msopprof模式用户指南](https://gitcode.com/Ascend/msopprof/blob/master/docs/zh/user_guide/msopprof_user_guide.md)。|

> [!NOTE]
>
> 同一个`visualize_data.bin`文件可同时包含多种可视化数据，导入后MindStudio Insight会自动识别并在对应界面展示。不同界面展示的数据内容取决于采集时使用的msopprof模式和参数。

**约束**

- MindStudio Insight在算子调优场景下，支持导入的json文件中，需要在文件内容中第一个中括号之前有"profilingType":"op"字段标识，才可被导入。由msOpProf工具生成的json文件默认包含此字段；若导入失败，请检查json文件内容是否包含此字段。
- 支持以文件夹方式导入json文件，一个文件夹下可存在多个子文件夹，但是子文件夹下不能存在多个json文件，不同的json文件需要放在不同的子文件夹下。
- 建议导入的json文件单文件大小不超过1GB。
- 支持导入的二进制bin文件只允许单个文件导入，不支持以文件夹方式导入。
- 建议导入的bin文件单文件大小不超过500MB。
- 仅Ascend 950PR&950DT系列产品支持通过“msopprof”方式采集指令流水图可视化呈现文件，数据展示在时间线（Timeline）界面。其他硬件型号可通过“msopprof simulator”仿真模式采集trace.json文件，在时间线界面查看指令流水。

**术语说明**

|术语|全称|说明|
|--|--|--|
|HBM|High Bandwidth Memory|高带宽存储器，AI处理器的外部主存储器，容量大但访问延迟较高。|
|GM|Global Memory|全局内存，即HBM，算子可访问的全局存储空间。|
|L2|L2 Cache|二级缓存，位于HBM和AI Core之间，用于缓存频繁访问的数据以降低访问延迟。|
|L1|L1 Buffer|一级缓存，AI Core内部的存储空间，供Cube和Vector计算单元共享。|
|L0A/L0B|L0A/L0B Buffer|Cube计算单元的输入缓冲区，分别存储矩阵乘的左矩阵和右矩阵。|
|L0C|L0C Buffer|Cube计算单元的输出缓冲区，存储矩阵乘的结果。|
|UB|Unified Buffer|统一缓冲区，Vector计算单元的专用存储空间，用于向量运算的数据输入输出。|
|MTE1|Memory Transfer Engine 1|数据搬运引擎1，搬运方向：L1 → {L0A, L0B, UB}。|
|MTE2|Memory Transfer Engine 2|数据搬运引擎2，搬运方向：{DDR/GM, L2} → {L1, L0A/B, UB}。|
|MTE3|Memory Transfer Engine 3|数据搬运引擎3，搬运方向：UB → {DDR/GM, L2, L1}、L1 → {DDR/L2}。|
|FIXP|Fix Pipeline|数据搬运流水，搬运方向：L0C → OUT/L1。|
|Pipe|Pipeline|指令流水线，AI Core中不同类型指令的执行通道。|
|Core|AI Core|AI处理器中的计算核心，包含Cube、Vector等计算单元。|
|Block|Sub Block|算子执行时的并行分块，每个Block在一个AI Core上运行。|
