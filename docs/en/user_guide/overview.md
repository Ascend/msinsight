# **Introduction**

## Overview

MindStudio Insight is a visualization tuning tool for Ascend developers. It supports system, operator, serving, and memory tuning, enable developers to quickly tune performance in training, inference, and operator development scenarios.

MindStudio Insight provides various tuning analysis methods, displays real software and hardware running data, analyzes performance bottlenecks from multiple dimensions, and supports visualized performance analysis for clusters with hundreds and thousands of cards, and even beyond, enabling developers to complete performance tuning within days.

## Advantage

- MindStudio Insight allows developers to view the profile data in the cluster scenario on the **Timeline** tab page, displays the data by single card, and automatically traverses all .db, **trace_view.json** files (in PyTorch and MindSpore scenarios), and **msprof*.json** files (in TensorFlow and offline inference scenarios) in the input path. They do not need to manually merge files.

- With the database, MindStudio Insight supports large-scale profile data processing, analysis of 20 GB cluster profile data, and performance tuning in foundation model scenarios.

## Feature Description

MindStudio Insight supports system, operator, serving, and memory tuning, and visualizes the data for display, enabling developers to quickly tune performance.

- System Tuning

  MindStudio Insight provides the timeline view, memory, operator duration, and communication bottleneck analysis to allow developers to quickly locate model performance bottlenecks and perform in-depth tuning.

  | Function Interface             | Description                                                        | Scenario Description                        |
  | --------------------- | ------------------------------------------------------------ | -------------------------------- |
  | Timeline|Displays the running status of the entire online inference and training process in the timeline view based on the scheduling process, and provides functions such as cluster timeline display and system view details viewing.|-                                |
  | Memory|Provides visualized display of memory information during collection. Displays the operator memory trend in an operator memory curves.|-                                |
  | Operator|Provides operator duration statistics and analysis.|-                                |
  | Summary|Displays the computing and communication operator duration analysis, and displays the analysis results in a bar chart, curve, and data pane.|PyTorch cluster scenario is supported.|
  | Communication|Displays the network link performance across the cluster and the communication performance of all nodes. By analyzing the overlapped duration between cluster communication and computation, slow hosts or nodes in the cluster training can be identified.|PyTorch cluster scenario is supported.|
  | RL|Performs high-level abstraction based on the collected data, and visualizes the timing relationships of the control flows. This helps to quickly identify time-consuming tasks and pipeline bubbles, and supports further performance analysis.|-                                |

- Operator Tuning

  MindStudio Insight provides the instruction pipeline view, operator source code view, and operator running load analysis view to display the key performance metrics of operators running on the Ascend AI Processor in a visualized manner, helping developers quickly locate software and hardware performance bottlenecks of operators and improve operator performance analysis efficiency.

  | Function Interface          | Description                                                        | Remarks                                    |
    | ------------------ | ------------------------------------------------------------ | ---------------------------------------- |
    | Timeline| Displays the running status of instructions on the Ascend AI Processor in a timeline view, displays the overall running status based on the scheduling process, and allows users to view instruction details and search for instructions.| -                                        |
    | Source    | Displays the operator instruction heatmap, and allows developers to view the mapping between the operator source code and instruction sets as well as the time consumption.| BIN files of operator profiling collected by msProf are supported.|
    | Details   | Displays the basic operator information, compute workload analysis, and memory workload analysis, as well as the analysis results in charts and data panes.| BIN files of operator profiling collected by msProf are supported.|
    | Cache     | Displays the L2 cache access of kernel functions in user programs, helping users optimize the cache hit rate.| BIN files of operator profiling collected by msProf are supported.|

- Serving tuning

  MindStudio Insight displays the end-to-end request execution in the timeline view, showing the duration of the request in each key phase and the status of the request. This helps users quickly identify service performance bottlenecks and adjust the tuning policy accordingly.

  | Function Interface          | Description                                                        | Scenario Description                               |
  | ------------------ | ------------------------------------------------------------ | --------------------------------------- |
  | Timeline| Displays the end-to-end request execution status in a timeline view, helping users intuitively view the duration of the request in each key phase and the current request status.| JSON files of trace data of inference service requests are supported.|
  | Curve   | Displays the end-to-end performance of the inference service process in a curve and a data details table.| The **profiler.db** file is supported.                  |

- Memory Tuning

  MindStudio Insight displays the detailed memory allocation on the device in graphics, and marks the usage details of various memory allocations based on the Python call stack and custom dotting tags to locate and optimize memory problems.

  | Function Interface         | Description                                                        | Scenario Description                                     |
  | ----------------- | ------------------------------------------------------------ | --------------------------------------------- |
  | msMemScope| Displays call stack diagrams, curve block charts, and memory breakdown diagrams to visualize memory usage, helping developers analyze and locate memory issues and effectively reduce diagnosis time.| Memory result files in .db format collected by msMemScope are supported.|
 
## Constraints

MindStudio Insight allows you to import and display profile data files in various formats and provides suggestions and restrictions on file specifications.

| File Type| Suggestions                                           | Specification Restrictions              |
| ----------- | --------------------------------------------- | ---------------------- |
| JSON| Recommended single file size: not exceed 1 GB. Recommended total file size: not exceed 20 GB.         | Single file size: Must not exceed 10 GB.|
| BIN | Recommended single file size: not exceed 500 MB.                                | Single file size: Must not exceed 10 GB.|
| DB  | &#8226; System tuning: Recommended single file size: not exceed 1 GB.<br> &#8226; Serving tuning: Recommended single file size: not exceed 1 GB.| &#8226; System tuning: Single file size: Must not exceed 20 GB.<br> &#8226; Serving tuning: Single file size: Must not exceed 10 GB.|
| CSV | CSV files are stored in text data. Recommended single file size: not exceed 500 MB.    | Single file size: Must not exceed 2 GB. |
