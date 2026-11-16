# **MindStudio Insight System Tuning**

## Overview

MindStudio Insight allows you to import profile data files and provides the timeline view, memory usage, operator duration, and communication bottleneck analysis to help developers quickly locate model performance bottlenecks.

## Preparations

**Environment Setup**

Install MindStudio Insight fi**Analysis Mode Description**rst. For details, see [MindStudio Insight Installation Guide](../../install_guide/mindstudio_insight_install_guide.md).

**Data Preparation**

Import profile data in the correct format. For details about the data, see [Data Description](#data-description). For details about how to import data, see [Importing Data](../basic_operations.md#importing-data).

## Data Description<a id="data-description"></a>

**Recommended Analysis Path**

The system tuning documentation is extensive. It is recommended to select a reading path based on the problem type.

| Problem Type | Recommended Reading Path |
| --- |----------------------------------------------------------------------------------------|
| Importing system profile data for the first time | [Data Description](#data-description) → [Import Data](../basic_operations.md#importing-data) → [Timeline](timeline.md#timeline) |
| Slow single-rank training/inference performance | [Timeline](timeline.md#timeline) → [Operator](operator.md#operator) → [Memory](memory.md#memory) |
| Slow cluster communication or fast/slow rank discrepancy | [Summary](summary.md#summary) → [Communication](communication.md#communication) → [Timeline](timeline.md#timeline) |
| Suspected host-side dispatch bottleneck | [Timeline](timeline.md#timeline) → [Host Bound Problem Analysis Case](host_bound_case.md#host-bound-analysis-case) |
| Reinforcement learning task idle gaps or abnormal phase duration | [Reinforcement Learning (RL)](rl.md#rl) |

**Overview**

For details about how to collect profile data files, see "Quick Start of Profiling in PyTorch Training Scenarios", "Quick Start of Profiling in TensorFlow Training Scenarios", and "Common msprof Collection Commands" in the *Profiling Tool Guide*, and "Debugging and Tuning" \> "[Ascend Performance Tuning](https://www.mindspore.cn/tutorials/en/r2.7.0/debug/profiler.html)" in the *[MindSpore Tutorial](https://www.mindspore.cn/tutorials/en/r2.7.0/index.html)*.

Profile data is classified into single-rank scenario and cluster scenario. For details, see [**Table 1** Performance data scenario description](#performance-data-scenario-description).

**Table 1** Performance data scenario description <a id="performance-data-scenario-description"></a>

|Scenario|Description|
|--|--|
|Single-rank scenario|You can import single-rank data into MindStudio Insight for analysis. When the single-rank data is imported, the **Timeline**, **Memory**, and **Operator** tab pages are displayed on MindStudio Insight.|
|Cluster scenario|- Clusters are classified into small clusters and large clusters based on the number of ranks. The GUI display varies depending on the imported data.<br> - Simplified cluster data. Cluster data is simplified to display only data of large communication operators and some computing operators.|

**Precautions**

- If the imported text profile data contains DB files, MindStudio Insight preferentially parses the DB files. If you only need to visualize the TEXT data, search for and delete the DB files in the original profile data folder, and import the data again.
- The profile data of system tuning and serving tuning can be imported at the same time. You need to place the data of the two scenarios in the same folder and select the folder when importing the data.
- The **memory\_record.csv** and **operator\_memory.csv** files must exist in the same directory. The **Memory** tab page can be properly displayed only after the files are successfully imported.
- When a single rank is imported, the **Summary** and **Communication** tab pages are not displayed.
- When the MindSpore training/inference data is collected in graph mode, the compilation optimization level parameter **jit\_level** is set to **O2**, and the profile data collected by calling the **step** API is imported to MindStudio Insight, the **Communication** tab page is not displayed.
- For the **PROF***\_*XXX** directory where the profile data is not parsed, you need to use the export function of the msprof command line to parse and export the profile data file before using MindStudio Insight to display the data. For details about how to use the msprof command line to parse and export data, see "msprof Model Tuning Tool" \> "Offline Parsing" in the *Profiling Tool Guide*.
- You can import the operator dotting data file. For details about how to obtain the file, see "msprof\_tx" in "Ascend PyTorch Profiler" in the *Profiling Tool Guide*. After the file is imported, the dotting data is displayed on **Timeline**.
- When cluster data is imported, if the profile data file contains the **cluster\_analysis\_output** directory file, related information is displayed on the **Summary** and **Communication** tab pages based on the **cluster\_analysis\_output** directory file after the import is successful. If the profile data file does not contain the **cluster\_analysis\_output** directory file, the corresponding **cluster\_analysis\_output** directory file is generated when data is imported to MindStudio Insight.
- In the cluster scenario, if the profile data collected by Ascend PyTorch Profiler or MindSpore Profiler needs to be displayed using MindStudio Insight, you are advised to set **repeat** to **1**. The value **0** is not recommended. If **repeat** is greater than **1**, the collected profile data folder needs to be divided into **repeat** equal parts. The files need to be stored in different folders based on the timestamp in the folder name and re-imported. In this way, the data can be properly displayed.
- If the msprof-analyze tool has been installed in Linux when you use MindStudio Insight to analyze cluster data, check the tool version and upgrade it to the latest version. For details about how to install the msprof-analyze tool of the latest version, see [msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/en/install_guide/msprof-analyze_install_guide.md#5-upgrade).
- A single JSON file containing ACLGraph graph construction process data can be imported.
- If both single-rank data and cluster data exist in the directory of the imported data, MindStudio Insight parses and displays only the cluster data.
- You can import both profile data (.text format and .db format) and ftrace data (.json format and .db format). First add a data project for one collection method, and then add data files from the other collection method within that project.
- Supports the visual display of NPU data collected by PyTorch Profiler (based on the mspti + kineto plugin). The file format is `.pt.trace.json`. For multi-rank scenarios, importing multiple files from the same directory in batches is not supported at present; only single-rank append import is supported: import the `.pt.trace.json` files of other ranks into an already imported single-rank `.pt.trace.json` project to visualize the data.

**Collection Configuration and Parameter Description**

When collecting profile data, different collection levels and parameter configurations directly affect the generated data files and the displayable pages and fields in the MindStudio Insight tool. Select an appropriate collection configuration based on your actual analysis requirements.

- For the mapping between profiling levels and generated data as well as displayable pages, see [Table 2 Mapping Between Profiling Levels and Data](#mapping-between-profiling-levels-and-data).
- For complete parameter configuration of the Ascend PyTorch Profiler API, see [_ExperimentalConfig parameter description](#experimentalconfig-parameter-description) and [torch_npu.profiler.profile main API parameter description](#torch_npu.profiler.profile-main-api-parameter-description).
- For the mapping between profiling parameters and page display fields, see [Collection parameter and page display field mapping](#collection-parameter-and-page-display-field-mapping).

**Table 2** Mapping between profiling levels and data<a id="mapping-between-profiling-levels-and-data"></a>

|Profiling Level|Generated Files|Displayable Interface|Description|
|--|--|--|--|
|ProfilerLevel.Level0|trace_view.json, msprof_*.json, operator_details.csv, kernel_details.csv (without AI Core performance metrics), memory_record.csv, operator_memory.csv|Timeline, Memory, Operator|Basic profiling level. Communication data and AI Core performance metrics are not collected.|
|ProfilerLevel.Level1|All Level0 files + communication.json, communication_matrix.json, kernel_details.csv (with AI Core performance metrics, requires the aic_metrics parameter)|All Level0 pages + Summary, Communication|Medium profiling level. Communication data and AI Core performance metrics are additionally collected.|
|ProfilerLevel.Level2|All Level1 files + memory copy data|All Level1 pages + Memcpy Overall|Full profiling level. Includes memory copy data.|

> [!NOTE]
>
> - When importing single-rank scenario data, the **Summary** and **Communication** views are not displayed even if the profiling level is **Level1** or **Level2**.
> - AI Core performance metrics in `kernel_details.csv` (such as `aicore_time`, `aic_mac_ratio`, etc.) are collected only when `aic_metrics=torch_npu.profiler.AiCMetrics.PipeUtilization` is configured and the profiling level is **Level1** or higher.
> - The **Memcpy Overall** view is displayed in the system view of the **Timeline** only when the profiling level is **Level2**.

**_ExperimentalConfig Parameter Description**

When using the Ascend PyTorch Profiler interface to collect profile data, you can configure collection parameters through `_ExperimentalConfig`. For parameter descriptions, see [Table 3 _ExperimentalConfig parameter description](#experimentalconfig-parameter-description).

**Table 3** _ExperimentalConfig parameter description<a id="experimentalconfig-parameter-description"></a>

|Name|Type|Default|Description|Impact on Displayable Interface|
|--|--|--|--|--|
|export_type|List[ExportType]|[Text, Db]|Export format. Supports Text and DB.|Data in DB format additionally displays the **Process** unit in the **Timeline**. The amount of information parsed from Text and DB formats differs.|
|profiler_level|ProfilerLevel|Level0|Profiling level. Options: Level_none, Level0, Level1, and Level2.|Different levels correspond to different generated files and displayable views. For details, see [Table 2 Correspondence Between Profiling Levels and Data](#mapping-between-profiling-levels-and-data).|
|aic_metrics|AiCMetrics|AiCoreNone|AI Core performance metric collection item. Options: AiCoreNone, PipeUtilization, etc.|When set to PipeUtilization and profiler_level is Level1 or higher, AI Core performance metric fields (such as aicore_time, aic_mac_ratio, etc.) are displayed in operator details.|
|msprof_tx|bool|False|Whether to enable msprof_tx trace collection.|When set to True, trace data is displayed in the **Timeline** view.|
|mstx_domain_include|List[str]|[]|Domains included in MSTX tracing.|Only MSTX trace data of the specified domains is displayed.|
|mstx_domain_exclude|List[str]|[]|Domains excluded from MSTX tracing.|MSTX trace data of the specified domains is excluded.|
|l2_cache|bool|False|Whether to collect L2 cache information.|When set to True, L2 cache-related fields are displayed in operator details.|
|op_attr|bool|False|Whether to collect operator attribute information.|When set to True and profiler_level is Level2, the Attr Info field is displayed in operator details.|
|data_simplification|bool|False|Whether to enable data simplification.|When enabled, collected data is simplified to reduce data volume.|
|record_op_args|bool|False|Whether to record operator arguments.|When set to True, the Args field is displayed in operator details.|
|gc_detect_threshold|int/None|None|GC detection threshold, in milliseconds.|When set, the Python GC unit is displayed in the **Timeline** view, showing GC events that exceed the threshold.|
|host_sys|List[HostSystem]|[]|Host-side system metric collection. Options: CPU, MEM.|When set, host-side CPU/memory utilization data units are displayed in the **Timeline** view.|
|sys_io|bool|False|Whether to collect system I/O information.|When set to True, system I/O-related data is displayed in the **Timeline** view.|
|sys_interconnection|bool|False|Whether to collect system interconnection information.|When set to True, system interconnection-related data is displayed in the Timeline view.|

**`torch_npu.profiler.profile` Main API Parameter Description**

For details about the parameters of the `torch_npu.profiler.profile` main API, see [Table 4 torch_npu.profiler.profile parameter description](#torch_npu.profiler.profile-main-api-parameter-description).

**Table 4 `torch_npu.profiler.profile` parameter description**<a id="torch_npu.profiler.profile-main-api-parameter-description"></a>

|Parameter|Description|Impact on Displayable Interface|
|--|--|--|
|activities|Type of events to collect. Options: `CPU` (framework side) and `NPU` (CANN software stack and NPU hardware layer).|`CPU`: The timeline displays Python/CANN layer API data. `NPU`: The timeline displays underlying NPU Task data.|
|schedule|Collection scheduling policy. Parameters include `wait` (number of waiting rounds), `warmup` (number of warmup rounds), `active` (number of collection rounds), `repeat` (number of repetitions), and `skip_first` (whether to skip the first round).|It is recommended to set `repeat` to `1`. When `repeat` > 1, the data folder must be split and re-imported.|
|record_shapes|Whether to record the input and output shapes of operators.|When set to `True`, the `Input Shapes` and `Output Shapes` fields are displayed in the operator details.|
|profile_memory|Whether to collect memory allocation information.|When set to `True`, `memory_record.csv` and `operator_memory.csv` are generated, and the **Memory** interface can be displayed normally.|
|with_stack|Whether to collect the Python call stack.|When set to `True`, call stack information is displayed in the operator details.|
|with_modules|Whether to collect module information.|When set to `True`, module information is displayed in the operator details.|
|with_flops|Whether to estimate FLOPS.|When set to `True`, the estimated FLOPS value is displayed in the operator details.|
|experimental_config|`_ExperimentalConfig` configuration object, used to configure collection parameters.|For details, see [_ExperimentalConfig Parameter Description](#experimentalconfig-parameter-description).|

**Correspondence Between Collection Parameters and Page Display Fields**

Different collection parameter configurations affect how fields are displayed on each page of MindStudio Insight. When certain fields on a page show N/A or are empty, it may be because the corresponding collection parameters are not configured. <a href="#collection-parameter-and-page-display-field-mapping">Table 5 Collection parameter and page display field mapping</a> lists the collection conditions required for key fields on each page.

**Table 5** Collection parameter and page display field mapping<a id="collection-parameter-and-page-display-field-mapping"></a>

|Page|Field|Collection Condition|
|--|--|--|
|Timeline - Slice Detail|Input Shapes|profiler_level >= Level0, and the operator is on an NPU accelerator core; record_shapes=True|
|Timeline - Slice Detail|Input Data Types|profiler_level >= Level0, and the operator is on an NPU accelerator core|
|Timeline - Slice Detail|Input Formats|profiler_level >= Level0, and the operator is on an NPU accelerator core|
|Timeline - Slice Detail|Output Shapes|profiler_level >= Level0, and the operator is on an NPU accelerator core; record_shapes=True|
|Timeline - Slice Detail|Output Data Types|profiler_level >= Level0, and the operator is on an NPU accelerator core|
|Timeline - Slice Detail|Output Formats|profiler_level >= Level0, and the operator is on an NPU accelerator core|
|Timeline - Slice Detail|Attr Info|op_attr=True and profiler_level >= Level2|
|Timeline - Slice Detail|Args|record_op_args=True|
|Timeline - Python GC Unit|GC Events|gc_detect_threshold is set|
|Timeline - Host-Side System Data Unit|CPU/Memory Utilization|host_sys includes CPU and/or MEM|
|Timeline - MSTX Mark Data|MSTX Events|msprof_tx=True or mstx_domain_include is not empty|
|Operator - Compute Operator Details|AI Core Performance Metrics (aicore_time, etc.)|aic_metrics=PipeUtilization and profiler_level >= Level1|
|Operator - Compute Operator Details|Input Shapes|profiler_level >= Level0, and the operator is on an NPU accelerator core|
|Operator - Compute Operator Details|Attr Info|op_attr=True and profiler_level >= Level2|
|Memory|Operator Memory Line Chart|profile_memory=True, and both memory_record.csv and operator_memory.csv exist|
|Summary|Parallel Strategy Analysis|profiler_level >= Level1, and communication data must exist|
|Summary|Memory Copy Overview|profiler_level = Level2|
|Communication|Communication Matrix|profiler_level >= Level1, and communication.json or communication_matrix.json must exist|
|Communication|Communication Duration Analysis|profiler_level >= Level1, and communication data must exist|

> [!NOTE]
>
> - When single-rank scenario data is imported, the **Summary** and **Communication** pages are not displayed, even if the collection level is **Level1** or **Level2**.
> - When task-time is configured as l0 (corresponding to `ProfilerLevel.Level0`) during data collection, fields such as Input Shapes and Input Data Types are not collected and are displayed as N/A.
> - When task-time is configured as l1 (corresponding to `ProfilerLevel.Level1`) during data collection, the Attr Info field is not collected and is displayed as N/A; this field is available only when aclnn is enabled and task-time is configured as l2 (corresponding to `ProfilerLevel.Level2`).

**Single-Rank Scenario**

In the single-rank scenario, profile data can be classified into the following types:

- PyTorch training/inference data: The profile data directory whose name ends with **ascend\_pt** can be imported. For details about the profile data files, see [**Table 6** PyTorch training/inference profile data files](#pytorch-training-inference-profile-data-files).

    **Table 6** PyTorch training/inference profile data files <a id="pytorch-training-inference-profile-data-files"></a>

    |File|Description|GUI|
    |--|--|--|
    |trace_view.json|Includes application layer data, CANN layer data, and low-level NPU data.|Timeline|
    |msprof_*.json|Indicates Timeline reports. If AI Core frequency data exists, the **AI Core Freq** layer is displayed.|Timeline|
    |operator_details.csv|Collects statistics on the duration of PyTorch operators on the host (delivery) and device (execution).|Timeline|
    |memory_record.csv|Indicates process-level memory allocation information.|Memory|
    |operator_memory.csv|Indicates operator memory allocation information.|Memory|
    |kernel_details.csv|Indicates information about all operators executed on the NPU.|Operator|
    |step_trace_time.csv|Indicates time statistics of computation and communication in a step.|Summary|
    |communication.json|Indicates the file that stores details about communication operators, such as communication duration and bandwidth.|Communication|
    |communication_matrix.json|Indicates the basic information file of small communication operators.|Communication|
    |ascend_pytorch_profiler_{*rank_id*}.db|Indicates the profile data file collected by Ascend PyTorch Profiler APIs.|Timeline<br> Memory<br> Operator<br> Summary<br> Communication|
    |analysis.db|Indicates data files collected in multi-rank or cluster scenarios involving communication.|Timeline<br> Memory<br> Operator<br> Summary<br> Communication|
    |Note: The asterisk (*) indicates the timestamp.|-|-|

- MindSpore training/inference data: MindSpore framework profile data can be imported. For details about how to obtain the data, see the "Debugging and Tuning > [Ascend Performance Tuning](https://www.mindspore.cn/tutorials/en/r2.7.0/debug/profiler.html)" chapter in the *[MindSpore Tutorial](https://www.mindspore.cn/tutorials/en/r2.7.0/index.html)*.

    MindStudio Insight allows you to import the profile data directory whose name ends with **ascend\_ms**. For details about the profile data files, see [Table 7 MindSpore training/inference profile data files](#mindspore-training-inference-profile-data-files).

    **Table 7** MindSpore training/inference profile data files <a id="mindspore-training-inference-profile-data-files"></a>

    |File|Description|GUI|
    |--|--|--|
    |msprof_*.json|Indicates Timeline reports. If AI Core frequency data exists, the **AI Core Freq** layer is displayed.|Timeline|
    |trace_view.json|Includes application layer data, CANN layer data, and low-level NPU data.|Timeline|
    |memory_record.csv|Indicates process-level memory allocation information.|Memory|
    |operator_memory.csv|Indicates operator memory allocation information.|Memory|
    |static_op_mem.csv|Indicates memory allocation information in static curve scenarios. If **static_op_mem.csv** exists, the **Memory** tab page displays the static curve mode.|Memory|
    |kernel_details.csv|Indicates information about all operators executed on the NPU.|Operator|
    |step_trace_time.csv|Indicates time statistics of computation and communication in a step.|Summary|
    |communication.json|Indicates the file that stores details about communication operators, such as communication duration and bandwidth.|Communication|
    |communication_matrix.json|Indicates the basic information file of small communication operators.|Communication|
    |Note: The asterisk (*) indicates the timestamp.|-|-|

- Offline inference data: The profile data in the `mindstudio_profiler_output` directory can be imported. For details about the profile data files, see [**Table 8** Offline inference profile data files](#offline-inference-profile-data-files).

    **Table 8** Offline inference profile data files<a id="offline-inference-profile-data-files"></a>

    |File|Description|GUI|
    |--|--|--|
    |msprof_*.json|Indicates Timeline reports.|Timeline|
    |fusion_op_*.csv|Indicates operator fusion summary in a model. This profile data file does not exist in single-operator scenarios.|Timeline|
    |api_statistic_*.csv|Indicates time spent by API execution at the CANN layer.|Timeline|
    |memory_record_*.csv|Indicates process-level memory allocation information.|Memory|
    |operator_memory_*.csv|Indicates operator memory allocation information.|Memory|
    |op_summary_*.csv|Indicates AI Core and AICPU operator data.|Operator|
    |op_statistic_*.csv|Indicates the number of times that the AI Core and AICPU operators are called and the time consumption.|Operator|
    |prof_rule_0_*.json|Indicates profiling suggestions.|Timeline<br>Summary<br>Communication|
    |step_trace_*.csv|Indicates step trace data. This profile data file does not exist in single-operator scenarios.|-|
    |step_trace_*.json|Indicates step trace data, which records the time required for each step. This profile data file does not exist in single-operator scenarios.|-|
    |task_time_*.csv|Indicates task scheduler data.|-|
    |msprof_*.db|Indicates a unified .db file. Currently, the data volume of this format is different from that of the data parsed by the TEXT format.|Timeline<br>Memory<br>Operator<br>Summary<br>Communication|
    |Note: The asterisk (*) indicates the timestamp.|-|-|

- npumonitor data: Supports importing profile data collected by npumonitor. npumonitor is a lightweight online monitoring tool based on the dynolog open-source project, supporting two modes: NPU Monitor (routine monitoring) and NPU Trace-dump (precise collection). For collection methods and installation guides, see:

  - [npumonitor Tool Feature Description](https://gitcode.com/Ascend/msmonitor/blob/master/docs/en/user_guide/npumonitor_instruct.md)
  - [msMonitor Product Documentation](https://gitcode.com/Ascend/msmonitor/blob/master/docs/en/quick_start/msmonitor_quick_start.md)
  - [msMonitor Tool Download](https://gitcode.com/Ascend/msmonitor/blob/master/docs/en/install_guide/msmonitor_install_guide.md)

  For details about profile data files, see <a href="#profile-data-file-details">Table 9 Profile data file details</a>.

    **Table 9** Profile data file details <a id="profile-data-file-details"></a>

    |File|Description|GUI|
    |--|--|--|
    |msmonitor_{pid}\_{timestamp}\_{rank_id}.db|DB file collected by npumonitor|Timeline|

    > [!NOTE]
    >
    > - *pid* indicates the process ID, and *timestamp* indicates the timestamp. For cluster data, *rank\_id* is a non-negative integer and starts from 0. For single-device data, *rank\_id* is **-1**.
    > - MindStudio Insight supports the import of a single DB file collected by npumonitor. You can also import the upper-level directory of the DB files, which will be displayed in tile mode. If the data volume is large, you are advised to import a single DB file each time. Importing all files at once can lead to slow parsing and potential out-of-memory errors.

**Cluster Scenario**

- The cluster scenario, also called the multi-rank scenario, refers to the cluster data composed of multiple multi-rank data. Cluster data can be classified into small cluster data and large cluster data. When MindStudio Insight is used to import data in different cluster scenarios, the data is different, as shown in [**Table 10** Cluster scenario description](#Cluster-scenario-description).

    **Table 10** Cluster scenario description <a id="Cluster-scenario-description"></a>

    |Scenario|Rank Count|Importing Data|GUI|
    |--|--|--|--|
    |Small cluster|Up to 32 ranks|All collected raw data can be imported.|Timeline<br>Memory<br>Operator<br>Summary<br>Communication|
    |Large cluster|More than 32 ranks, thousands of ranks, and tens of thousands of ranks.|Use the cluster analysis capability of msprof-analyze in the mstt toolset to preprocess the raw profile data, obtain the communication group-based communication analysis and step duration analysis results, and import the preprocessed data.<br> For details about how to download and use the msprof-analyze tool, see [msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/en/install_guide/msprof-analyze_install_guide.md).<br> 1. Save all directories whose names end with **ascend_pt** or **ascend_ms** to the same folder.<br> 2. Use the msprof-analyze tool to generate the communication-related **cluster_analysis_output** directory. For details about the data files in this directory, see [**Table 18** Files in the cluster\_analysis\_output directory](#directory-files).<br> 3. Copy the generated **cluster_analysis_output** directory to the local PC and import the directory to MindStudio Insight.<br> 4. Go to the **Communication** tab page, analyze the data, import the corresponding small cluster data or single-rank data, and analyze the data again.|Summary<br>Communication|

    In a large cluster scenario, directly importing all raw data collected by the performance tuning tool is not recommended, as the parsing process is time-consuming.

**Cluster Data Collection and Preprocessing Process**<a id="cluster-data-collection-and-preprocessing-process"></a>

In a cluster scenario, you need to first use the performance tuning tool to collect profile data from each rank, then use the msprof-analyze tool to preprocess the multi-rank data, generate cluster analysis data, and import it into the MindStudio Insight tool for visual display.

> The following content is sourced from the *[msprof-analyze Tool](https://gitcode.com/Ascend/msprof-analyze/blob/master/README.md)* document. In case of updates, refer to the source document.

1. Install the msprof-analyze tool.

   The msprof-analyze tool is included in the MSTT (MindStudio Tuning Toolset). For installation instructions, see the msprof-analyze tool documentation.

2. Use the performance tuning tool to collect profile data from each rank. For the collection method, see the *Ascend PyTorch Profiler Tool Collection Guide*.

3. Use the msprof-analyze tool to preprocess multi-rank data and generate cluster analysis data.

   **Common commands of the msprof-analyze tool**

   |Platform|Command|
   |--|--|
   |Windows|`cluster_analysis.exe -d <data directory> -m <analysis mode>`|
   |Linux|`python3 cluster_analysis.py -d <data directory> -m <analysis mode>`|
   |Mac|`cluster_analysis -d <data directory> -m <analysis mode>`|

   If the collected data is in DB format, the `--data_simplification` option must be additionally added.

   **Analysis Mode Description**

   |Mode|Description|Generated Files|
   |--|--|--|
   |all|Generates all cluster analysis data.|cluster_step_trace_time.csv, cluster_communication_matrix.json, cluster_communication.json, communication_group.json (TEXT format) or cluster_analysis.db (DB format)|
   |communication_time|Generates only communication time analysis.|cluster_communication.json, communication_group.json (TEXT format) or cluster_analysis.db (DB format)|
   |communication_matrix|Generates only communication matrix analysis.|cluster_step_trace_time.csv, cluster_communication_matrix.json, communication_group.json (TEXT format) or cluster_analysis.db (DB format)|
   |filter_db|Generates cluster streamlined data, extracting only large communication operators, key computing functions, and key framework functions.|cluster_analysis.db (DB format)|

4. Import the generated cluster analysis data into the MindStudio Insight tool.

   - TEXT format: Place the `cluster_step_trace_time.csv`, `cluster_communication_matrix.json`, `cluster_communication.json`, and `communication_group.json` files in the same directory as the raw data of each rank, and then import them.
   - DB format: Place the `cluster_analysis.db` file in the same directory as the raw data of each Rank, and then import it.

> [!NOTE]
>
> - In large-scale cluster scenarios, it is recommended to use the filter_db mode to generate streamlined data before importing, so as to reduce the data volume and parsing time.
> - The cluster analysis data must be in the same directory as the raw data of each rank; otherwise, MindStudio Insight cannot correctly associate the cluster data.
> - For detailed parameter descriptions of the msprof-analyze tool, see the msprof-analyze tool documentation.

**AI Core/AI Vector Core Performance Metric Field Description**

When `aic_metrics=torch_npu.profiler.AiCMetrics.PipeUtilization` is set in the collection configuration and the profiler_level is Level1 or above, the AI Core and AI Vector Core performance metric fields are displayed in the operator details. These metrics help users analyze the computational bottlenecks of operators, as shown in [Table 11 AI Core performance metric field description](#ai-core-performance-metric-field-description) and [Table 12 AI Vector Core performance metric field description](#ai-vector-core-performance-metric-field-description).

**Table 11** AI Core performance metric field description<a id="ai-core-performance-metric-field-description"></a>

|Field|Description|Interpretation|
|--|--|--|
|aicore_time(μs)|AI Core Execution Time|The actual execution time of the operator on the AI Core, excluding wait time.|
|aic_total_cycles|AI Core Total Cycles|The total number of clock cycles executed, which can be used to estimate instruction execution efficiency.|
|aic_mac_time(μs)|MAC Unit Duration|The duration of the matrix multiply unit. The MAC unit is responsible for matrix multiply-accumulate operations.|
|aic_mac_ratio|MAC Unit Ratio|MAC duration/total duration. A high value indicates a compute-intensive operator with high compute resource utilization.|
|aic_scalar_time(μs)|Scalar Unit Duration|The duration of the scalar processing unit. The Scalar is responsible for control flow and scalar operations.|
|aic_scalar_ratio|Scalar Unit Ratio|Scalar duration/total duration. A high value may indicate complex control logic.|
|aic_mte1_time(μs)|Memory Transfer Engine 1 (MTE1) Duration|The duration of MTE1, which is responsible for reading data from the L1 cache.|
|aic_mte1_ratio|MTE1 Ratio|MTE1 duration/total duration. A high value indicates frequent L1 cache reads.|
|aic_mte2_time(μs)|MTE2 Duration|The duration of MTE2, which is responsible for reading data from DDR/L2 to L1.|
|aic_mte2_ratio|MTE2 Ratio|MTE2 duration/total duration. A high value indicates a memory bandwidth bottleneck, and the operator is memory-bound.|
|aic_fixpipe_time(μs)|FixPipe Unit Duration|The duration of the data post-processing unit, which is responsible for format conversion and precision processing.|
|aic_fixpipe_ratio|FixPipe Unit Ratio|FixPipe duration/total duration.|
|aic_icache_miss_rate|AI Core iCache Miss Rate|The instruction cache miss rate. A high value indicates a low instruction cache hit rate, and instruction layout optimization may be required.|

**Table 12** AI Vector Core performance metric field description<a id="ai-vector-core-performance-metric-field-description"></a>

|Field|Description|Interpretation|
|--|--|--|
|aiv_time(μs)|AI Vector Execution Time|Actual execution time of the operator on the AI Vector Core.|
|aiv_total_cycles|AI Vector Total Cycles|Total number of clock cycles executed.|
|aiv_vec_time(μs)|Vector Unit Duration|Duration of the vector computing unit.|
|aiv_vec_ratio|Vector Unit Ratio|Vector duration / total duration. A high value indicates vector computation-intensive workload.|
|aiv_scalar_time(μs)|Vector Scalar Unit Duration|Duration of the vector scalar processing unit.|
|aiv_scalar_ratio|Vector Scalar Ratio|Vector Scalar duration/total duration.|
|aiv_mte2_time(μs)|Vector MTE2 Duration|Duration of Vector MTE2, which is responsible for reading data from DDR/L2.|
|aiv_mte2_ratio|Vector MTE2 Ratio|Vector MTE2 duration / total duration. A high value indicates a memory bandwidth bottleneck.|
|aiv_mte3_time(μs)|Vector MTE3 Duration|Duration of Vector MTE3, which is responsible for writing data back to DDR/L2.|
|aiv_mte3_ratio|Vector MTE3 Ratio|Vector MTE3 duration / total duration.|
|aiv_icache_miss_rate|AI Vector iCache Miss Rate|Vector instruction cache miss rate.|
|cube_utilization(%)|Cube Utilization|Matrix multiplication unit utilization percentage, reflecting the usage efficiency of the Cube unit.|

> [!NOTE]
>
> - AI Core performance metrics apply to operators whose Accelerator Core is AI_CORE, and AI Vector Core performance metrics apply to operators whose Accelerator Core is AI_VECTOR_CORE or MIX_AIV.
> - When aic_metrics is set to AiCoreNone (default), the above fields are not collected and are displayed as N/A in MindStudio Insight.
> - By analyzing the MAC/MTE2 ratio, you can determine whether an operator is compute-bound or memory-bound: a high MAC ratio indicates compute-bound, while a high MTE2 ratio indicates memory-bound.

**Raw Data File Field Definitions**<a id="raw-data-file-field-definitions"></a>

The raw profile data files imported by the MindStudio Insight tool contain multiple CSV and JSON files. The key field definitions of each file are shown in [Table 13 kernel_details.csv field definitions](#kernel_detailscsv-field-definitions) through [Table 16 memory_record.csv field definitions](#memory_recordcsv-field-definitions).

**Table 13** `kernel_details.csv` field definitions<a id="kernel_detailscsv-field-definitions"></a>

The `kernel_details.csv` file records information about all operators executed on the NPU. The field definitions are listed in the following table.

|Field|Description|
|--|--|
|Step Id|Iteration number.|
|Model ID|Model ID.|
|Task ID|Task ID.|
|Stream ID|Stream ID.|
|Name|Operator name.|
|Type|Operator type (such as Conv2D, MatMulV2, and TransData).|
|OP State|Operator state (such as dynamic).|
|Accelerator Core|Accelerator core (AI_CORE, AI_VECTOR_CORE, DSA_SQE, MIX_AIV).|
|Start Time(μs)|Start time (in microseconds).|
|Duration(μs)|Duration (in microseconds).|
|Wait Time(μs)|Wait time (in microseconds).|
|Block Dim|Block dimension.|
|Mix Block Dim|Mixed block dimension.|
|HF32 Eligible|Whether HF32 is supported (YES/NO).|
|Input Shapes|Input shapes.|
|Input Data Types|Input data types.|
|Input Formats|Input data formats (NCHW, NC1HWC0, FRACTAL_Z, FORMAT_ND, etc.).|
|Output Shapes|Output shapes.|
|Output Data Types|Output data types.|
|Output Formats|Output data formats.|
|Context ID|Context ID.|
|aicore_time(μs) ~ aic_icache_miss_rate|AI Core performance metric fields. The configuration `aic_metrics=PipeUtilization` and `profiler_level` >= `Level1` is required. For details, see [Table 11 AI Core Performance Metric Field Description](#ai-core-performance-metric-field-description).|
|aiv_time(μs) ~ cube_utilization(%)|AI Vector Core performance metric fields. The configuration `aic_metrics=PipeUtilization` and `profiler_level` >= `Level1` is required. For details, see [Table 12 AI Vector Core Performance Metric Field Description](#ai-vector-core-performance-metric-field-description).|

**Table 14** `step_trace_time.csv` field definitions<a id="step_trace_timecsv-field-definitions"></a>

The `step_trace_time.csv` file records the time statistics of computing and communication in iterations. The field definitions are listed in the following table.

|Field|Description|
|--|--|
|Step|Iteration number.|
|Computing|Computing time.|
|Communication(Not Overlapped)|Non-overlapped communication time.|
|Overlapped|Overlapped time between computing and communication.|
|Communication|Total communication time.|
|Free|Free time.|
|Stage|Stage time.|
|Bubble|Bubble time (waiting time in pipeline parallelism scenarios).|
|Communication(Not Overlapped and Exclude Receive)|Non-overlapped communication time excluding receive.|
|Preparing|Preparing time.|

**Table 15** `operator_memory.csv` field definitions**<a id="operator_memorycsv-field-definitions"></a>

The `operator_memory.csv` file records operator memory allocation details. The field definitions are listed in the following table.

|Field|Description|
|--|--|
|Name|Operator name.|
|Size(KB)|Memory size (KB).|
|Allocation Time(μs)|Allocation time.|
|Release Time(μs)|Release time.|
|Active Release Time(μs)|Active release time.|
|Duration(μs)|Duration.|
|Active Duration(μs)|Active duration.|
|Allocation Total Allocated(MB)|Total allocated memory at allocation.|
|Allocation Total Reserved(MB)|Total Reserved memory at allocation.|
|Allocation Total Active(MB)|Total active memory at allocation.|
|Release Total Allocated(MB)|Total allocated memory at release.|
|Release Total Reserved(MB)|Total reserved memory at release.|
|Release Total Active(MB)|Total active memory at release.|
|Stream Ptr|Stream pointer.|
|Device Type|Device type (for example, NPU:1).|

**Table 16** `memory_record.csv` field definitions<a id="memory_recordcsv-field-definitions"></a>

The `memory_record.csv` file records process-level memory allocation details. The field definitions are listed in the following table.

|Field|Description|
|--|--|
|Component|Component type (APP/PTA/GE/PTA+GE)|
|Timestamp (μs)|Timestamp|
|Total Allocated (MB)|Total allocated memory|
|Total Reserved (MB)|Total reserved memory|
|Total Active (MB)|Total active memory|
|Stream Ptr|Stream pointer|
|Device Type|Device type|

**DB Format and TEXT Format Differences**<a id="db-format-and-text-format-differences"></a>

The MindStudio Insight tool supports importing profile data in TEXT format (CSV/JSON files) and DB format (SQLite database files). The two formats differ in data content and display, as shown in [Table 17 DB Format and TEXT Format Differences](#db-format-and-text-format-differences).

**Table 17** DB Format and TEXT Format Differences<a id="db-format-and-text-format-differences"></a>

|Comparison Item|TEXT Format|DB Format|
|--|--|--|
|File Form|CSV and JSON files, one file per data type|SQLite database file (.db), with all data stored in a unified database|
|Data Completeness|Some data fields are not collected in TEXT format|DB format collects more complete data, including more API layers and call chain information|
|Timeline Display|Displays application layer, CANN layer, and underlying NPU data|Additionally displays the Process unit; supports PYTORCH_API, CANN_API, OSRT_API, and other layer data|
|Operator Display|Displays operator information based on kernel_details.csv|Displays based on the COMPUTE_TASK_INFO table, with additional fields such as attrInfo and waitNs|
|Communication Display|Based on communication.json and communication_matrix.json|Based on the COMMUNICATION_OP and COMMUNICATION_TASK_INFO tables, with additional fields such as transportType, linkType, and rdmaType|
|MSTX Marker Data|Not supported|Supported, displayed based on the MSTX_EVENTS table|
|Python Call Stack|Not supported|Supported, displayed based on the PYTORCH_CALLCHAINS table|
|Memory Copy Information|Not supported|Supported, displayed based on the MEMCPY_INFO table|
|GC Record|Not supported|Supported, displayed based on the GC_RECORD table|
|Data Parsing Speed|Faster parsing for small data volumes|More efficient parsing for large data volumes|
|Recommended Scenario|Single-rank scenarios and scenarios with small data volumes|Multi-rank/cluster scenarios and scenarios requiring complete data information|

> [!NOTE]
>
> - When export_type includes both Text and DB, MindStudio Insight preferentially parses data in DB format.
> - Data tables in DB format (such as PYTORCH_API, CANN_API, TASK, etc.) are internal data structures. Users do not need to operate them directly; MindStudio Insight automatically parses and displays them.
> - It is recommended to export both Text and Db formats simultaneously when collecting profile data (i.e., export_type=[Text, Db]) for use in different analysis scenarios.

**Table 18** Files in the `cluster\_analysis\_output` directory<a id="directory-files"></a>

|File|Description|
|--|--|
|cluster_step_trace_time.csv|Generated when the data parsing mode is **communication_matrix**, **communication_time**, or **all**.|
|cluster_communication_matrix.json|Generated when the data parsing mode is **communication_matrix** or **all**.|
|cluster_communication.json|Generated when the data parsing mode is **communication_time** or **all**. The data is mainly the communication time data.|
|cluster_analysis.db|Generated during the parsing of **analysis.db** or **ascend_pytorch_profiler_***{rank_id}***.db**.|

- The cluster data is simplified based on the **ascend\_pytorch\_profiler\_***\{*rank\_id*\}***.db** file. Large communication operators, key compute functions, and key framework functions are extracted to simplify the data, saving memory and enabling quick global analysis. After the simplified cluster data is imported, only the **Timeline** tab page is displayed in MindStudio Insight.

    The msprof-analyze tool in the MSTT tool set can be used to generate reduced cluster data by setting **-m filter_db**. For details about how to install the msprof-analyze tool, see [Installing msprof-analyze](https://gitcode.com/Ascend/msprof-analyze/blob/master/docs/en/install_guide/msprof-analyze_install_guide.md). For details about how to set **-m filter_db**, see "filter_db" in [Table Structure Description of the Recipe Result and cluster_analysis.db Deliverable](https://gitcode.com/ascend/mstt/blob/pre-research/profiler/msprof_analyze/docs/recipe_output_format.md#filter_db). The cluster data reduction function supports only the unified DB scenario.
