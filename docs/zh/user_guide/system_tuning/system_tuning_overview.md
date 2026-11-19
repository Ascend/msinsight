# **MindStudio Insight系统调优**

## 简介

MindStudio Insight支持导入性能数据文件，提供时间线视图、内存、算子耗时、通信瓶颈分析等功能，以图形化形式呈现相关内容，帮助开发者快速定位模型性能瓶颈。

## 使用前准备

**环境准备**

请先安装MindStudio Insight工具，具体安装步骤请参见[MindStudio Insight安装指南](../../install_guide/mindstudio_insight_install_overview.md)。

**数据准备**

请导入正确格式的性能数据，具体数据说明请参见[数据说明](#数据说明)，数据导入操作请参见[导入数据](../basic_operations.md#导入数据)。

## 数据说明<a id="数据说明"></a>

**推荐分析路径**

系统调优文档内容较多，建议根据问题类型选择阅读路径：

| 问题类型 | 推荐阅读路径                                                                                 |
| --- |----------------------------------------------------------------------------------------|
| 首次导入系统性能数据 | [数据说明](#数据说明) → [导入数据](../basic_operations.md#导入数据) → [时间线（Timeline）](timeline.md#timeline)        |
| 单卡训练/推理性能慢 | [时间线（Timeline）](timeline.md#timeline) → [算子（Operator）](operator.md#operator) → [内存（Memory）](memory.md#memory)         |
| 集群通信慢或存在快慢卡 | [概览（Summary）](summary.md#summary) → [通信（Communication）](communication.md#communication) → [时间线（Timeline）](timeline.md#timeline) |
| 怀疑 Host 侧下发瓶颈 | [时间线（Timeline）](timeline.md#timeline) → [Host Bound问题分析案例](host_bound_case.md#host-bound-analysis-case)             |
| 强化学习任务空泡或阶段耗时异常 | [强化学习（RL）](rl.md#rl)                                                                        |

**概述**

性能数据文件的采集方式请分别参见《性能调优工具指南》中的“PyTorch训练场景性能分析快速入门”、“TensorFlow训练场景性能分析快速入门”和“msprof采集通用命令”章节内容，以及《[MindSpore教程](https://www.mindspore.cn/tutorials/zh-CN/r2.7.0/index.html)》中的“调试调优 \>  [Ascend性能调优](https://www.mindspore.cn/tutorials/zh-CN/r2.7.0/debug/profiler.html)”章节内容。

性能数据分为单卡场景和集群场景，具体请参见[**表 1**  性能数据场景说明](#性能数据场景说明)。

**表 1** 性能数据场景说明<a id="性能数据场景说明"></a>

|场景|说明|
|--|--|
|单卡场景|可在MindStudio Insight工具中导入单卡进行分析。当导入单卡场景数据时，MindStudio Insight工具支持显示时间线（Timeline）、内存（Memory）和算子（Operator）界面，具体内容请参见单卡场景。|
|集群场景|- 集群场景根据卡数量可分为小集群场景和大集群场景，导入不同场景的数据，界面展示也会有所变化，具体请参见集群场景。<br> - 集群精简数据，将集群数据简化，只显示通信大算子数据和部分计算类算子。|

**注意事项**

- 当导入的text场景的性能数据中同时存在db文件，MindStudio Insight工具会优先解析db文件。如果您只需要可视化呈现text场景数据，则需要在性能数据原文件夹中查找db文件并删除，再重新导入，即可呈现text场景数据。
- 支持同时导入系统调优和服务化调优的性能数据，需将两个场景的数据置于同一文件夹中，导入时选择该文件夹即可。
- memory\_record.csv和operator\_memory.csv两个文件必须同时存在且保证在同一目录，导入成功后内存（Memory）界面才能正常展示。
- 导入单卡时，不展示概览（Summary）和通信（Communication）界面。
- 当采集MindSpore训练/推理数据时，在GRAPH模式下，编译优化等级参数jit\_level设置为O2，且调用step接口方式采集的性能数据，在导入MindStudio Insight工具时，不支持展示通信（Communication）界面。
- 对于未完成性能数据解析的PROF\__XXX_目录，需要先使用msprof命令行的export功能解析并导出性能数据文件后才可以使用MindStudio Insight工具展示，数据使用msprof命令行解析并导出的操作请参见《性能调优工具指南》中的“msprof模型调优工具 \> 离线解析”章节。
- 支持导入算子打点数据文件，获取文件方式请参见《性能调优工具指南》中的“Ascend PyTorch Profiler”章节“msprof\_tx”相关内容，导入成功后会在时间线（Timeline）界面展示打点数据。
- 当导入集群数据时，如果性能数据文件中包含cluster\_analysis\_output目录文件，导入成功后，概览（Summary）和通信（Communication）界面会根据cluster\_analysis\_output目录文件内容呈现相关信息；如果性能数据文件中不包含cluster\_analysis\_output目录文件，在MindStudio Insight工具中导入数据时，会生成对应的cluster\_analysis\_output目录文件。
- 在集群场景下，使用Ascend PyTorch Profiler接口或者MindSpore Profiler接口采集到的性能数据，需要使用MindStudio Insight工具显示，则建议配置repeat=1，不推荐配置为0。如果repeat\>1，则需要将采集的性能数据文件夹分为repeat等份，按照文件夹名称中的时间戳先后将文件分别放到不同文件夹下重新导入，才可正常展示。
- 在Linux环境下使用MindStudio Insight工具分析集群场景数据时，如果已经安装了msprof-analyze工具，请检查版本并将其升级至最新版本，最新版本的msprof-analyze工具安装可参考[msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/zh/install_guide/msprof-analyze_install_guide.md#5-%E5%8D%87%E7%BA%A7)。
- 支持导入含有ACLGraph构图过程数据的单个json文件。
- 当导入数据的目录中同时存在单卡数据和集群数据，MindStudio Insight仅支持解析集群数据并进行可视化展示。
- 支持profiling采集数据（.text格式和.db格式）与ftrace_tools转换的独立Ftrace数据（.json格式和.db格式）联合导入，先添加其中一个采集方式的数据工程，再从该工程中添加另一个采集方式的数据文件。
- 支持随msprof profiling DB自动导入其中的Host Ftrace，无需额外转换或单独追加Ftrace文件。
- 支持导入由PyTorch Profiler（基于mspti + kineto插件）采集的NPU数据可视化展示，文件格式为`.pt.trace.json`。针对多卡场景，目前暂不支持将多个文件放入同一目录下批量导入，仅支持“单卡追加导入”，在已导入的单卡`.pt.trace.json`项目中追加导入其他卡的`.pt.trace.json`文件实现数据可视化。

**采集配置与参数说明**

采集性能数据时，不同的采集等级和参数配置会直接影响生成的数据文件和MindStudio Insight工具中可展示的页面及字段。请根据实际分析需求，选择合适的采集配置。

- 采集等级与生成数据、展示页面的对应关系请参见[表 2 采集等级与数据对应关系](#采集等级与数据对应关系)。
- Ascend PyTorch Profiler接口的完整参数配置请参见[_ExperimentalConfig参数说明](#experimentalconfig参数说明)和[torch_npu.profiler.profile主接口参数说明](#torch_npu.profiler.profile主接口参数说明)。
- 采集参数与页面展示字段的关联关系请参见[采集参数与页面展示字段对应关系](#采集参数与页面展示字段对应关系)。

**表 2** 采集等级与数据对应关系<a id="采集等级与数据对应关系"></a>

|采集等级|生成文件|可展示界面|说明|
|--|--|--|--|
|ProfilerLevel.Level0|trace_view.json、msprof_*.json、communication.json、operator_details.csv、kernel_details.csv（无AI Core性能指标）、memory_record.csv、operator_memory.csv|时间线（Timeline）、内存（Memory）、算子（Operator）、概览（Summary）、通信（Communication）|基础采集级别，采集部分通信数据和AI Core性能指标。|
|ProfilerLevel.Level1|Level0全部文件 + communication.json、communication_matrix.json、kernel_details.csv（含AI Core性能指标，需配合aic_metrics参数）|Level0全部 + 概览（Summary）、通信（Communication）|中等采集级别，额外采集通信数据和AI Core性能指标。|
|ProfilerLevel.Level2|Level1全部文件 + 内存拷贝数据|Level1全部 + 内存拷贝总览（Memcpy Overall）|完整采集级别，包含内存拷贝数据。|

> [!NOTE]
>
> - 当导入单卡场景数据时，即使采集等级为Level1或Level2，也不展示概览（Summary）和通信（Communication）界面。
> - kernel_details.csv中的AI Core性能指标（如aicore_time、aic_mac_ratio等）需要同时配置`aic_metrics=torch_npu.profiler.AiCMetrics.PipeUtilization`且采集等级为Level1及以上才会采集。
> - 内存拷贝总览（Memcpy Overall）仅在采集等级为Level2时才会在时间线（Timeline）的系统视图中展示。

**_ExperimentalConfig参数说明**

使用Ascend PyTorch Profiler接口采集性能数据时，可通过_ExperimentalConfig配置采集参数，参数说明如[表 3 _ExperimentalConfig参数说明](#experimentalconfig参数说明)所示。

**表 3** _ExperimentalConfig参数说明<a id="experimentalconfig参数说明"></a>

|参数|类型|默认值|说明|对展示界面的影响|
|--|--|--|--|--|
|export_type|List[ExportType]|[Text, Db]|导出格式，支持Text和Db两种。|Db格式数据在Timeline中额外展示Process泳道；Text和Db格式解析的数据信息量存在差异。|
|profiler_level|ProfilerLevel|Level0|采集等级，可选Level_none、Level0、Level1、Level2。|不同等级对应不同的生成文件和展示界面，详见[表 2 采集等级与数据对应关系](#采集等级与数据对应关系)。|
|aic_metrics|AiCMetrics|AiCoreNone|AI Core性能指标采集项，可选AiCoreNone、PipeUtilization等。|设置为PipeUtilization且profiler_level为Level1及以上时，算子详情中展示AI Core性能指标字段（如aicore_time、aic_mac_ratio等）。|
|msprof_tx|bool|False|是否启用msprof_tx打点采集。|设置为True时，时间线（Timeline）界面展示打点数据。|
|mstx_domain_include|List[str]|[]|MSTX打点包含的域。|仅展示指定域的MSTX打点数据。|
|mstx_domain_exclude|List[str]|[]|MSTX打点排除的域。|排除指定域的MSTX打点数据。|
|l2_cache|bool|False|是否采集L2 Cache信息。|设置为True时，算子详情中展示L2 Cache相关字段。|
|op_attr|bool|False|是否采集算子属性信息。|设置为True且profiler_level为Level2时，算子详情中展示Attr Info字段。|
|data_simplification|bool|False|是否启用数据精简。|启用后将精简采集数据，减少数据量。|
|record_op_args|bool|False|是否记录算子参数。|设置为True时，算子详情中展示Args字段。|
|gc_detect_threshold|int/None|None|GC检测阈值，单位为毫秒。|设置后，时间线（Timeline）界面展示Python GC泳道，显示超过阈值的GC事件。|
|host_sys|List[HostSystem]|[]|Host侧系统指标采集，可选CPU、MEM。|设置后，时间线（Timeline）界面展示Host侧CPU/内存利用率数据泳道。|
|sys_io|bool|False|是否采集系统IO信息。|设置为True时，时间线（Timeline）界面展示系统IO相关数据。|
|sys_interconnection|bool|False|是否采集系统互连信息。|设置为True时，时间线（Timeline）界面展示系统互连相关数据。|

**torch_npu.profiler.profile主接口参数说明**

torch_npu.profiler.profile主接口的参数说明如[表 4 torch_npu.profiler.profile参数说明](#torch_npu.profiler.profile主接口参数说明)所示。

**表 4** torch_npu.profiler.profile参数说明<a id="torch_npu.profiler.profile主接口参数说明"></a>

|参数|说明|对展示界面的影响|
|--|--|--|
|activities|采集事件类型，可选CPU（框架侧）和NPU（CANN软件栈及NPU硬件层）。|CPU：时间线展示Python/CANN层API数据；NPU：时间线展示底层NPU Task数据。|
|schedule|采集调度策略，参数包括wait（等待轮数）、warmup（预热轮数）、active（采集轮数）、repeat（重复次数）、skip_first（跳过首轮）。|repeat建议配置为1，repeat>1时需拆分数据文件夹重新导入。|
|record_shapes|是否记录算子输入输出shape。|设置为True时，算子详情中展示Input Shapes和Output Shapes字段。|
|profile_memory|是否采集内存分配信息。|设置为True时，生成memory_record.csv和operator_memory.csv，内存（Memory）界面可正常展示。|
|with_stack|是否采集Python调用栈。|设置为True时，算子详情中展示调用栈信息。|
|with_modules|是否采集模块信息。|设置为True时，算子详情中展示模块信息。|
|with_flops|是否估算FLOPS。|设置为True时，算子详情中展示FLOPS估算值。|
|experimental_config|_ExperimentalConfig配置对象，用于配置采集参数。|详见[_ExperimentalConfig参数说明](#experimentalconfig参数说明)。|

**采集参数与页面展示字段对应关系**

不同采集参数配置会影响MindStudio Insight各页面中字段的展示情况。当页面中某些字段显示为N/A或为空时，可能是因为未配置对应的采集参数。[表 5 采集参数与页面展示字段对应关系](#采集参数与页面展示字段对应关系)列出了各页面关键字段所需的采集条件。

**表 5** 采集参数与页面展示字段对应关系<a id="采集参数与页面展示字段对应关系"></a>

|页面|字段|采集条件|
|--|--|--|
|时间线（Timeline）- 选中详情|Input Shapes|profiler_level >= Level0，且为NPU加速核上的算子；record_shapes=True|
|时间线（Timeline）- 选中详情|Input Data Types|profiler_level >= Level0，且为NPU加速核上的算子|
|时间线（Timeline）- 选中详情|Input Formats|profiler_level >= Level0，且为NPU加速核上的算子|
|时间线（Timeline）- 选中详情|Output Shapes|profiler_level >= Level0，且为NPU加速核上的算子；record_shapes=True|
|时间线（Timeline）- 选中详情|Output Data Types|profiler_level >= Level0，且为NPU加速核上的算子|
|时间线（Timeline）- 选中详情|Output Formats|profiler_level >= Level0，且为NPU加速核上的算子|
|时间线（Timeline）- 选中详情|Attr Info|op_attr=True 且 profiler_level >= Level2|
|时间线（Timeline）- 选中详情|Args|record_op_args=True|
|时间线（Timeline）- Python GC泳道|GC事件|gc_detect_threshold已设置|
|时间线（Timeline）- Host侧系统数据泳道|CPU/内存利用率|host_sys包含CPU和/或MEM|
|时间线（Timeline）- MSTX打点数据|MSTX事件|msprof_tx=True或mstx_domain_include非空|
|算子（Operator）- 计算算子详情|AI Core性能指标（aicore_time等）|aic_metrics=PipeUtilization 且 profiler_level >= Level1|
|算子（Operator）- 计算算子详情|Input Shapes|profiler_level >= Level0，NPU加速核上的算子|
|算子（Operator）- 计算算子详情|Attr Info|op_attr=True 且 profiler_level >= Level2|
|内存（Memory）|算子内存折线图|profile_memory=True，且memory_record.csv和operator_memory.csv同时存在|
|概览（Summary）|并行策略分析|profiler_level >= Level0，需存在通信数据|
|概览（Summary）|内存拷贝总览|profiler_level = Level2|
|通信（Communication）|通信矩阵|profiler_level >= Level1，需存在communication.json或communication_matrix.json|
|通信（Communication）|通信耗时分析|profiler_level >= Level0，需存在通信数据|

> [!NOTE]
>
> - 当导入单卡场景数据时，概览（Summary）和通信（Communication）界面不展示，即使采集等级为Level1或Level2。
> - 当采集数据时task-time配置为l0（对应ProfilerLevel.Level0），Input Shapes、Input Data Types等字段不采集，显示为N/A。
> - 当采集数据时task-time配置为l1（对应ProfilerLevel.Level1），Attr Info字段不采集，显示为N/A；只有开启aclnn，task-time配置为l2（对应ProfilerLevel.Level2）时，才有此字段。

**单卡场景**

在单卡场景下，性能数据可分为三大类型，如下所示：

- PyTorch训练/推理数据：支持导入以“ascend\_pt”结尾的性能数据目录，性能数据文件详情请参见[**表 6** PyTorch训练/推理性能数据文件](#PyTorch训练/推理性能数据文件)。

    **表 6** PyTorch训练/推理性能数据文件<a id="PyTorch训练/推理性能数据文件"></a>

    |文件名|说明|展示界面|
    |--|--|--|
    |trace_view.json|包括应用层数据、CANN层数据和底层NPU数据。|时间线（Timeline）|
    |msprof_*.json|Timeline数据总表。如果存在变频数据（AI Core Freq）信息，会展示AI Core Freq层级。|时间线（Timeline）|
    |operator_details.csv|统计PyTorch算子在Host侧（下发）和Device侧（执行）的耗时。|时间线（Timeline）|
    |memory_record.csv|进程级内存申请情况信息。|内存（Memory）|
    |operator_memory.csv|算子内存申请情况信息。|内存（Memory）|
    |kernel_details.csv|NPU上执行的所有算子的信息。|算子（Operator）|
    |step_trace_time.csv|迭代中计算和通信的时间统计。|概览（Summary）|
    |communication.json|通信算子通信耗时、带宽等详细信息文件。|通信（Communication）|
    |communication_matrix.json|通信小算子基本信息文件。|通信（Communication）|
    |ascend_pytorch_profiler_{*rank_id*}.db|Ascend PyTorch Profiler接口采集的性能数据文件。|时间线（Timeline）<br> 内存（Memory）<br> 算子（Operator）<br> 概览（Summary）<br> 通信（Communication）|
    |analysis.db|多卡或集群等存在通信的场景下，采集到的数据文件。|时间线（Timeline）<br> 内存（Memory）<br> 算子（Operator）<br> 概览（Summary）<br> 通信（Communication）|
    |注：“*”表示{timestamp}时间戳。|||

- MindSpore训练/推理数据：支持导入MindSpore框架性能数据，获取方式请参见《[MindSpore教程](https://www.mindspore.cn/tutorials/zh-CN/r2.7.0/index.html)》中的“调试调优 \> [Ascend性能调优](https://www.mindspore.cn/tutorials/zh-CN/r2.7.0/debug/profiler.html)”章节。

    MindStudio Insight工具支持导入以“ascend\_ms”结尾的性能数据目录，性能数据文件详情请参见[**表 7** MindSpore训练/推理性能数据文件](#MindSpore训练/推理性能数据文件)。

    **表 7** MindSpore训练/推理性能数据文件<a id="MindSpore训练/推理性能数据文件"></a>

    |文件名|说明|展示界面|
    |--|--|--|
    |msprof_*.json|Timeline数据总表。如果存在变频数据（AI Core Freq）信息，会展示AI Core Freq层级。|时间线（Timeline）|
    |trace_view.json|包括应用层数据、CANN层数据和底层NPU数据。|时间线（Timeline）|
    |memory_record.csv|进程级内存申请情况信息。|内存（Memory）|
    |operator_memory.csv|算子内存申请情况信息。|内存（Memory）|
    |static_op_mem.csv|静态图场景内存申请情况信息。当static_op_mem.csv存在时，内存（Memory）界面会展示静态图模式。|内存（Memory）|
    |kernel_details.csv|NPU上执行的所有算子的信息。|算子（Operator）|
    |step_trace_time.csv|迭代中计算和通信的时间统计。|概览（Summary）|
    |communication.json|通信算子通信耗时、带宽等详细信息文件。|通信（Communication）|
    |communication_matrix.json|通信小算子基本信息文件。|通信（Communication）|
    |注：“*”表示{timestamp}时间戳。|||

- 离线推理数据：支持导入mindstudio\_profiler\_output目录下性能数据，性能数据文件详情请参见[**表 8** 离线推理性能数据文件](#离线推理性能数据文件)。

    **表 8** 离线推理性能数据文件<a id="离线推理性能数据文件"></a>

    |文件名|说明|展示界面|
    |--|--|--|
    |msprof_*.json|Timeline数据总表。|时间线（Timeline）|
    |fusion_op_*.csv|模型中算子融合前后信息。单算子场景下无此性能数据文件。|时间线（Timeline）|
    |api_statistic_*.csv|用于统计CANN层的API执行耗时信息。|时间线（Timeline）|
    |memory_record_*.csv|进程级内存申请情况信息。|内存（Memory）|
    |operator_memory_*.csv|算子内存申请情况信息。|内存（Memory）|
    |op_summary_*.csv|AI Core和AI CPU算子数据。|算子（Operator）|
    |op_statistic_*.csv|AI Core和AI CPU算子调用次数及耗时统计。|算子（Operator）|
    |prof_rule_0_*.json|调优建议。|时间线（Timeline）<br>概览（Summary）<br>通信（Communication）|
    |step_trace_*.csv|迭代轨迹数据。单算子场景下无此性能数据文件。|-|
    |step_trace_*.json|迭代轨迹数据，每轮迭代的耗时。单算子场景下无此性能数据文件。|-|
    |task_time_*.csv|Task Scheduler任务调度信息。|-|
    |msprof_*.db|统一db文件。当前该格式数据与text参数解析的数据信息量存在差异。|时间线（Timeline）<br>内存（Memory）<br>算子（Operator）<br>概览（Summary）<br>通信（Communication）|
    |注：“*”表示{timestamp}时间戳。|||

- npumonitor数据：支持导入npumonitor采集的性能数据。npumonitor是基于dynolog开源项目的轻量化在线监测工具，支持NPU Monitor（常态监测）和NPU Trace-dump（精准采集）两种模式，采集方式和安装指南请参见：

  - 《[npumonitor工具特性说明](https://gitcode.com/Ascend/msmonitor/blob/master/docs/zh/user_guide/npumonitor_instruct.md)》
  - 《[msMonitor产品文档](https://gitcode.com/Ascend/msmonitor/blob/master/docs/zh/quick_start/msmonitor_quick_start.md)》
  - 《[msMonitor工具下载](https://gitcode.com/Ascend/msmonitor/blob/master/docs/zh/install_guide/msmonitor_install_guide.md)》

  性能数据文件详情请参见[表 9 性能数据文件详情表](#性能数据文件详情表)。

    **表 9** 性能数据文件详情表<a id="性能数据文件详情表"></a>

    |文件名|说明|展示界面|
    |--|--|--|
    |msmonitor_{pid}\_{timestamp}\_{rank_id}.db|npumonitor采集的db文件。|时间线（Timeline）|

    > [!NOTE]
    >
    > - pid为进程号，timestamp为时间戳。如果是集群数据，rank\_id为非负整数，从0排序；如果是单卡数据，rank\_id为-1。
    > - MindStudio Insight支持导入npumonitor采集的单个db文件；也支持导入db文件的上一级目录，目录中多个db文件平铺展示。在数据量大的情况下，建议导入单个db文件，如果全部导入，数据解析耗时较长，可能会引发OOM（Out of Memory，内存溢出）问题。

**集群场景**

- 集群场景也称多卡场景，是指由多个单卡组成的集群数据，集群数据可分为小集群和大集群，MindStudio Insight工具导入不同场景的数据时，也有所不同，如[表 10 集群场景说明](#集群场景说明)所示。

    **表 10** 集群场景说明<a id="集群场景说明"></a>

    |场景|卡数量|导入数据|界面展示|
    |--|--|--|--|
    |小集群|不超过32卡。|可导入采集到的全部原始数据。|时间线（Timeline）<br>内存（Memory）<br>算子（Operator）<br>概览（Summary）<br>通信（Communication）|
    |大集群|超过32卡，千卡，万卡等。|采用mstt工具集中的msprof-analyze的集群分析能力预处理的原始性能数据，可得到基于通信域的通信分析和迭代耗时分析，导入预处理后得到的数据。<br> msprof-analyze工具的下载与使用请参见[msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/zh/install_guide/msprof-analyze_install_guide.md)。<br> 1. 将所有以“ascend_pt”或“ascend_ms”结尾的目录汇总至同一文件夹。<br> 2. 使用msprof-analyze工具生成通信相关文件“cluster_analysis_output”目录，“cluster_analysis_output”目录中数据文件请参见[**表 18** cluster\_analysis\_output目录文件](#目录文件)。<br> 3. 将生成的“cluster_analysis_output”目录文件拷贝至本地，并导入MindStudio Insight工具。<br> 4. 可先前往通信（Communication）界面分析后，导入对应小集群数据或者单卡数据，再次仔细分析。|概览（Summary）<br>通信（Communication）|

    如果在大集群场景下，直接导入性能调优工具采集的全部原始数据，解析耗时较长，不建议直接导入。

**集群数据采集与预处理流程**<a id="集群数据采集与预处理流程"></a>

集群场景下，需要先使用性能调优工具采集各Rank的性能数据，再通过msprof-analyze工具对多Rank数据进行预处理，生成集群分析数据后导入MindStudio Insight工具进行可视化展示。

> 以下内容来源于《[msprof-analyze工具](https://gitcode.com/Ascend/msprof-analyze/blob/master/README.md)》文档，如有更新请以源文档为准。

1. 安装msprof-analyze工具。

   msprof-analyze工具包含在MSTT（MindStudio Tuning Toolset）工具集中，安装方式请参见msprof-analyze工具文档。

2. 使用性能调优工具采集各Rank的性能数据，采集方式请参见《Ascend PyTorch Profiler工具采集指南》。

3. 使用msprof-analyze工具对多Rank数据进行预处理，生成集群分析数据。

   **msprof-analyze工具常用命令**

   |平台|命令|
   |--|--|
   |Windows|`cluster_analysis.exe -d <数据目录> -m <分析模式>`|
   |Linux|`python3 cluster_analysis.py -d <数据目录> -m <分析模式>`|
   |Mac|`cluster_analysis -d <数据目录> -m <分析模式>`|

   若采集数据为DB格式，需额外添加`--data_simplification`选项。

   **分析模式说明**

   |模式|说明|生成文件|
   |--|--|--|
   |all|生成全部集群分析数据|cluster_step_trace_time.csv、cluster_communication_matrix.json、cluster_communication.json、communication_group.json（TEXT格式）或cluster_analysis.db（DB格式）|
   |communication_time|仅生成通信耗时分析|cluster_communication.json、communication_group.json（TEXT格式）或cluster_analysis.db（DB格式）|
   |communication_matrix|仅生成通信矩阵分析|cluster_step_trace_time.csv、cluster_communication_matrix.json、communication_group.json（TEXT格式）或cluster_analysis.db（DB格式）|
   |filter_db|生成集群精简数据，仅提取大通信算子、关键计算函数和关键框架函数|cluster_analysis.db（DB格式）|

4. 将生成的集群分析数据导入MindStudio Insight工具。

   - TEXT格式：将cluster_step_trace_time.csv、cluster_communication_matrix.json、cluster_communication.json、communication_group.json文件与各Rank原始数据放在同一目录下导入。
   - DB格式：将cluster_analysis.db文件与各Rank原始数据放在同一目录下导入。

> [!NOTE]
>
> - 在大型集群场景下，建议使用filter_db模式生成精简数据后再导入，以减少数据量和解析耗时。
> - 集群分析数据必须与各Rank原始数据在同一目录下，否则MindStudio Insight无法正确关联集群数据。
> - msprof-analyze工具的详细参数说明请参见msprof-analyze工具文档。

**AI Core/AI Vector Core性能指标字段说明**

当采集配置中设置了`aic_metrics=torch_npu.profiler.AiCMetrics.PipeUtilization`且profiler_level为Level1及以上时，计算算子详情中会展示AI Core和AI Vector Core性能指标字段。这些指标可以帮助用户分析算子的计算瓶颈，如[表 11 AI Core性能指标字段说明](#ai-core性能指标字段说明)和[表 12 AI Vector Core性能指标字段说明](#ai-vector-core性能指标字段说明)所示。

**表 11** AI Core性能指标字段说明<a id="ai-core性能指标字段说明"></a>

|字段|说明|含义解读|
|--|--|--|
|aicore_time(μs)|AI Core执行时间|算子在AI Core上的实际执行时间，不包含等待时间。|
|aic_total_cycles|AI Core总周期数|执行总时钟周期数，可用于估算指令执行效率。|
|aic_mac_time(μs)|MAC单元耗时|矩阵乘单元耗时，MAC单元负责矩阵乘加运算。|
|aic_mac_ratio|MAC单元占比|MAC耗时/总耗时，高值表示计算密集型算子，计算资源利用率高。|
|aic_scalar_time(μs)|Scalar单元耗时|标量处理单元耗时，Scalar负责控制流和标量运算。|
|aic_scalar_ratio|Scalar单元占比|Scalar耗时/总耗时，高值可能表示控制逻辑较复杂。|
|aic_mte1_time(μs)|MTE1耗时|内存传输引擎1耗时，负责从L1缓存读取数据。|
|aic_mte1_ratio|MTE1占比|MTE1耗时/总耗时，高值表示L1缓存读取频繁。|
|aic_mte2_time(μs)|MTE2耗时|内存传输引擎2耗时，负责从DDR/L2读取数据到L1。|
|aic_mte2_ratio|MTE2占比|MTE2耗时/总耗时，高值表示内存带宽瓶颈，算子为内存bound。|
|aic_fixpipe_time(μs)|FixPipe单元耗时|数据后处理单元耗时，负责格式转换和精度处理。|
|aic_fixpipe_ratio|FixPipe单元占比|FixPipe耗时/总耗时。|
|aic_icache_miss_rate|AI Core iCache未命中率|指令缓存未命中率，高值表示指令缓存命中率低，可能需要优化指令布局。|

**表 12** AI Vector Core性能指标字段说明<a id="ai-vector-core性能指标字段说明"></a>

|字段|说明|含义解读|
|--|--|--|
|aiv_time(μs)|AI Vector执行时间|算子在AI Vector Core上的实际执行时间。|
|aiv_total_cycles|AI Vector总周期数|执行总时钟周期数。|
|aiv_vec_time(μs)|Vector单元耗时|向量计算单元耗时。|
|aiv_vec_ratio|Vector单元占比|Vector耗时/总耗时，高值表示向量计算密集。|
|aiv_scalar_time(μs)|Vector Scalar单元耗时|向量标量处理单元耗时。|
|aiv_scalar_ratio|Vector Scalar占比|Vector Scalar耗时/总耗时。|
|aiv_mte2_time(μs)|Vector MTE2耗时|向量内存传输引擎2耗时，负责从DDR/L2读取数据。|
|aiv_mte2_ratio|Vector MTE2占比|Vector MTE2耗时/总耗时，高值表示内存带宽瓶颈。|
|aiv_mte3_time(μs)|Vector MTE3耗时|向量内存传输引擎3耗时，负责将数据写回DDR/L2。|
|aiv_mte3_ratio|Vector MTE3占比|Vector MTE3耗时/总耗时。|
|aiv_icache_miss_rate|AI Vector iCache未命中率|向量指令缓存未命中率。|
|cube_utilization(%)|Cube利用率|矩阵乘单元利用率百分比，反映Cube单元的使用效率。|

> [!NOTE]
>
> - AI Core性能指标适用于Accelerator Core为AI_CORE的算子，AI Vector Core性能指标适用于Accelerator Core为AI_VECTOR_CORE或MIX_AIV的算子。
> - 当aic_metrics设置为AiCoreNone（默认值）时，上述字段不会采集，在MindStudio Insight中显示为N/A。
> - 通过分析MAC/MTE2占比，可以判断算子是计算bound还是内存bound：MAC占比高为计算bound，MTE2占比高为内存bound。

**原始数据文件字段定义**<a id="原始数据文件字段定义"></a>

MindStudio Insight工具导入的原始性能数据文件中包含多个CSV和JSON文件，各文件的关键字段定义如[表 13 kernel_details.csv字段定义](#kernel_detailscsv字段定义)至[表 16 memory_record.csv字段定义](#memory_recordcsv字段定义)所示。

**表 13** kernel_details.csv字段定义<a id="kernel_detailscsv字段定义"></a>

kernel_details.csv文件记录NPU上执行的所有算子信息，字段定义如下：

|字段|说明|
|--|--|
|Step Id|迭代编号|
|Model ID|模型ID|
|Task ID|任务ID|
|Stream ID|流ID|
|Name|算子名称|
|Type|算子类型（如Conv2D、MatMulV2、TransData等）|
|OP State|算子状态（如dynamic）|
|Accelerator Core|加速器核心（AI_CORE、AI_VECTOR_CORE、DSA_SQE、MIX_AIV）|
|Start Time(μs)|开始时间（微秒）|
|Duration(μs)|持续时间（微秒）|
|Wait Time(μs)|等待时间（微秒）|
|Block Dim|块维度|
|Mix Block Dim|混合块维度|
|HF32 Eligible|是否支持HF32（YES/NO）|
|Input Shapes|输入形状|
|Input Data Types|输入数据类型|
|Input Formats|输入数据格式（NCHW、NC1HWC0、FRACTAL_Z、FORMAT_ND等）|
|Output Shapes|输出形状|
|Output Data Types|输出数据类型|
|Output Formats|输出数据格式|
|Context ID|上下文ID|
|aicore_time(μs) ~ aic_icache_miss_rate|AI Core性能指标字段，需配置aic_metrics=PipeUtilization且profiler_level >= Level1，详见[表 11 AI Core性能指标字段说明](#ai-core性能指标字段说明)|
|aiv_time(μs) ~ cube_utilization(%)|AI Vector Core性能指标字段，需配置aic_metrics=PipeUtilization且profiler_level >= Level1，详见[表 12 AI Vector Core性能指标字段说明](#ai-vector-core性能指标字段说明)|

**表 14** step_trace_time.csv字段定义<a id="step_trace_timecsv字段定义"></a>

step_trace_time.csv文件记录迭代中计算和通信的时间统计，字段定义如下：

|字段|说明|
|--|--|
|Step|迭代编号|
|Computing|计算时间|
|Communication(Not Overlapped)|非重叠通信时间|
|Overlapped|计算与通信重叠时间|
|Communication|总通信时间|
|Free|空闲时间|
|Stage|阶段时间|
|Bubble|气泡时间（流水线并行场景下的等待时间）|
|Communication(Not Overlapped and Exclude Receive)|排除接收的非重叠通信时间|
|Preparing|准备时间|

**表 15** operator_memory.csv字段定义**<a id="operator_memorycsv字段定义"></a>

operator_memory.csv文件记录算子内存申请情况，字段定义如下：

|字段|说明|
|--|--|
|Name|算子名称|
|Size(KB)|内存大小（KB）|
|Allocation Time(μs)|分配时间|
|Release Time(μs)|释放时间|
|Active Release Time(μs)|活跃释放时间|
|Duration(μs)|持续时间|
|Active Duration(μs)|活跃持续时间|
|Allocation Total Allocated(MB)|分配时总已分配内存|
|Allocation Total Reserved(MB)|分配时总预留内存|
|Allocation Total Active(MB)|分配时总活跃内存|
|Release Total Allocated(MB)|释放时总已分配内存|
|Release Total Reserved(MB)|释放时总预留内存|
|Release Total Active(MB)|释放时总活跃内存|
|Stream Ptr|流指针|
|Device Type|设备类型（如NPU:1）|

**表 16** memory_record.csv字段定义<a id="memory_recordcsv字段定义"></a>

memory_record.csv文件记录进程级内存申请情况，字段定义如下：

|字段|说明|
|--|--|
|Component|组件类型（APP/PTA/GE/PTA+GE）|
|Timestamp(μs)|时间戳|
|Total Allocated(MB)|总已分配内存|
|Total Reserved(MB)|总预留内存|
|Total Active(MB)|总活跃内存|
|Stream Ptr|流指针|
|Device Type|设备类型|

**DB格式与TEXT格式差异说明**<a id="db格式与text格式差异说明"></a>

MindStudio Insight工具支持导入TEXT格式（CSV/JSON文件）和DB格式（SQLite数据库文件）的性能数据。两种格式在数据内容和展示上存在差异，如[表 17 DB格式与TEXT格式差异](#db格式与text格式差异)所示。

**表 17** DB格式与TEXT格式差异<a id="db格式与text格式差异"></a>

|对比项|TEXT格式|DB格式|
|--|--|--|
|文件形式|CSV和JSON文件，每个数据类型一个文件|SQLite数据库文件（.db），所有数据存储在统一数据库中|
|数据完整性|部分数据字段在TEXT格式中不采集|DB格式采集的数据更完整，包含更多API层级和调用链信息|
|时间线（Timeline）展示|展示应用层、CANN层和底层NPU数据|额外展示Process泳道；支持PYTORCH_API、CANN_API、OSRT_API等层级数据|
|算子（Operator）展示|基于kernel_details.csv展示算子信息|基于COMPUTE_TASK_INFO表展示，额外包含attrInfo、waitNs等字段|
|通信（Communication）展示|基于communication.json和communication_matrix.json|基于COMMUNICATION_OP和COMMUNICATION_TASK_INFO表，额外包含transportType、linkType、rdmaType等字段|
|MSTX打点数据|不支持|支持，基于MSTX_EVENTS表展示|
|Python调用栈|不支持|支持，基于PYTORCH_CALLCHAINS表展示|
|内存拷贝信息|不支持|支持，基于MEMCPY_INFO表展示|
|GC记录|不支持|支持，基于GC_RECORD表展示|
|数据解析速度|数据量较小时解析较快|数据量大时解析效率更高|
|推荐场景|单卡场景、数据量较小的场景|多卡/集群场景、需要完整数据信息的场景|

> [!NOTE]
>
> - 当export_type同时包含Text和Db时，MindStudio Insight优先解析DB格式数据。
> - DB格式中的数据表（如PYTORCH_API、CANN_API、TASK等）为内部数据结构，用户无需直接操作，MindStudio Insight会自动解析并展示。
> - 建议在采集性能数据时同时导出Text和Db格式（即export_type=[Text, Db]），以便在不同分析场景下使用。

**表 18** cluster\_analysis\_output目录文件<a id="目录文件"></a>

|文件名|说明|
|--|--|
|cluster_step_trace_time.csv|数据解析模式为communication_matrix、communication_time或all时均生成。|
|cluster_communication_matrix.json|数据解析模式为communication_matrix或all时生成。|
|cluster_communication.json|数据解析模式为communication_time或all时生成，主要为通信耗时数据。|
|cluster_analysis.db|解析analysis.db或ascend_pytorch_profiler_{*rank_id*}.db生成的文件。|

- 集群精简数据，是基于ascend\_pytorch\_profiler\_\{_rank\_id_\}.db文件，提取通信类大算子数据，计算类关键函数和框架关键函数，将数据精简，节约内存，快速进行全局分析，导入集群精简数据后，MindStudio Insight工具只显示时间线（Timeline）界面。

    集群数据精简可使用mstt工具集中的msprof-analyze工具，通过设置`-m filter_db`生成集群精简数据，msprof-analyze工具安装可参考《[安装msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/zh/install_guide/msprof-analyze_install_guide.md)》，设置`-m filter_db`可参考《[recipe结果和cluster\_analysis.db交付件表结构说明](https://gitcode.com/ascend/mstt/blob/pre-research/profiler/msprof_analyze/docs/recipe_output_format.md#filter_db)》中的“filter\_db”内容，集群数据精简功能只支持统一db场景。
