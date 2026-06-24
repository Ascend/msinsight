<h1 align="center">MindStudio Insight</h1>
<div align="center">
  <img src="./modules/framework/public/favicon.ico" width="200" alt="MindStudio Insight Logo">
  <p>🚀 <b>All-scenario Ascend AI visualization profiling tool</b></p>

  [![Ask DeepWiki](https://badgen.net/badge/Ask/DeepWiki/blue)](https://deepwiki.com/qianxiaoxixixi/MsInsightForEveryOne/ ) [![Ask ZRead](https://badgen.net/badge/Ask/ZRead/orange)](https://zread.ai/qianxiaoxixixi/MsInsightForEveryOne) [![doc](https://badgen.net/badge/doc/readthedocs/green)](https://msinsight.readthedocs.io/zh-cn/latest/)
  [![License](https://badgen.net/badge/License/MulanPSL-2.0/blue)](./License) [![Version](https://badgen.net/badge/Version/8.3.0/green)](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_8.3.0) [![Ascend](https://img.shields.io/badge/Hardware-Ascend-orange.svg)](https://www.hiascend.com/)
</div>

## 🌟 What's New

- **[2026-02-04]** 🎉 **MindStudio Insight 26.0.0-alpha.1 is now available!** This version focuses on the **host bound** issue and **RL** performance analysis and tuning.

## 📌 Overview

**MindStudio Insight** is an advanced visualization tuning and analysis tool designed for Ascend AI developers. It displays real software and hardware running data in a visualized manner, helping developers accurately locate and resolve performance bottlenecks within days.

### Core Values

- **All-scenario coverage:** supports system tuning, operator tuning, serving tuning, and memory tuning.
- **Ultra-large scale cluster support:** easily handles cluster analysis with hundreds or thousands of ranks and can process up to **20 GB+** of profile data.
- **Simplified operations:** automatically traverses profile data, eliminating the need for manual file merging and supporting plug-and-play.

### Demo

<div style="
  padding: 4px 0 2px 0;            /* Extremely narrow top and bottom */
  font-weight: 600;                 Semi-bold
  color: #1a2639;
  letter-spacing: 1px;
">
  System Tuning
</div>
<div style="padding: 0;">
  <img style="width: 100%; height: auto; display: block;" alt="Importing large files" src="./assets/demo-system.gif" />
</div>

<details>
<summary>🔍 Directory structure </summary>

```tex
├── build                              # Build script
├── docs                               # Project documentation
├── e2e                                # Test cases
├── modules                            # Module directory
│   ├── build                          # Build script
│   ├── cluster                        # Summary and Communication
│   ├── compute                        # Operator tuning module
│   ├── framework                      # Frontend main framework module
│   ├── leaks                          # Memory leak module
│   ├── lib                            # Public directory
│   ├── memory                         # Memory module
│   ├── operator                       # Operator module
│   ├── reinforcement-learning         # RL module
│   ├── statistic                      # Serving tuning module
│   ├── timeline                       # Timeline module
├── platform                           # Base directory
├── plugins                            # Plugin directory
├── scripts                            # Script directory
├── server                             # Backend service module
│   ├── build                          # Build script
│   ├── cmake                          # Build script for open-source software
│   ├── src
│   │   ├── channel                    # Network communication
│   │   ├── defs                       # Global definition
│   │   ├── entry                       # Compilation module
│   │   │   ├── server
│   │   │      ├── bin                 # Server module
│   │   ├── protocol                   # Message definition
│   │   ├── module
│   │   │   ├── base                   # Base class shared by modules
│   │   │   ├── global                 # Global message
│   │   │   ├── timeline               # Timeline message processing
│   │   │   │   ├── core               # Core processing logic
│   │   │   │   ├── handler            # Message processing
│   │   │   │   ├──protocol            # Message format conversion
│   │   │   ├── ...
│   │   ├── server                     # Server service
│   │   ├── utils                      # Utility class
│   ├── third_party                    # Open-source software
```

</details>

## 🔖 Version Description

| Release Version| Release Date       | Release Tag      | Compatibility   |
| ------- | --------------- | ------------- | ------------- |
| 26.0.0-alpha.1 | 2026/02/04  | tag_MindStudio_26.0.0-alpha.1 | Compatible with Ascend CANN 8.5.0 and earlier versions. For details about how to obtain the CANN installation package, see [CANN Installation Guide](https://www.hiascend.com/cann).|

## 🛠️ Installation

MindStudio Insight can be installed and used on Windows, Linux, and macOS systems, and can be installed as a plugin. For details about the installation procedure, see [MindStudio Insight Installation Guide](./docs/en/user_guide/mindstudio_insight_install_guide.md).

## 🚀 Quick Start

- [System Tuning](./docs/zh/quick_start/system_tuning_quick_start.md): Learn how to use the **Summary**, **Communication**, and **Timeline** tabs to analyze the model system performance.
- [Operator Tuning](./docs/zh/quick_start/operator_tuning_quick_start.md): Learn how to use the **Details**, **Timeline**, and **Source** tabs to analyze operator performance.

---

## ⚠️ Constraints and Precautions

MindStudio Insight allows you to import and display profile data files in various formats and provides suggestions and restrictions on file specifications.

| File Type| Suggestions                                           | Specification Restrictions              |
| ----------- | --------------------------------------------- | ---------------------- |
| JSON| Recommended single file size: not exceed 1 GB. Recommended total file size: not exceed 20 GB.         | Single file size: Must not exceed 20 GB.|
| BIN | Recommended single file size: not exceed 500 MB.                                | Single file size: Must not exceed 20 GB.|
| DB  | - System tuning: Keep files under 1 GB each.<br> - Serving tuning: Keep files under 1 GB each.| - System tuning: Keep files under 20 GB each.<br> - Serving tuning: Keep files under 10 GB each.|
| CSV | CSV files are stored in text data. Recommended single file size: not exceed 500 MB.    | Single file size: Must not exceed 2 GB. |

## 💻 Basic Operations

The basic operations of MindStudio Insight include basic settings, data import, and shortcut keys. For details, see [Basic Operations of MindStudio Insight](./docs/en/user_guide/basic_operations.md).

## Feature Description

MindStudio Insight supports system, operator, serving, and memory tuning, and visualizes the data for display, enabling developers to quickly tune performance.

- System Tuning

  MindStudio Insight provides the timeline view, memory, operator duration, and communication bottleneck analysis to allow developers to quickly locate model performance bottlenecks and perform in-depth tuning. For details, see [System Tuning](./docs/en/user_guide/system_tuning.md).

  | Function Interface             | Description                                                        | Scenario Description                        |
  | --------------------- | ------------------------------------------------------------ | -------------------------------- |
  | Timeline   | Displays the running status of the entire online inference and training process in the timeline view based on the scheduling process, and provides functions such as cluster timeline display and system view details viewing.| -                                |
  | Memory       | Provides visualized display of memory information during collection. Displays the operator memory trend in an operator memory curves.| -                                |
  | Operator     | Provides operator duration statistics and analysis.                                    | -                                |
  | Summary      | Displays the computing and communication operator duration analysis, and displays the analysis results in a bar chart, curve, and data pane.| PyTorch cluster scenario is supported.|
  | Communication| Displays the network link performance across the cluster and the communication performance of all nodes. By analyzing the overlapped duration between cluster communication and computation, slow hosts or nodes in the cluster training can be identified.| PyTorch cluster scenario is supported.|
  | Reinforcement Learning (RL)       | Performs high-level abstraction based on the collected data, and visualizes the timing relationships of the control flows. This helps to quickly identify time-consuming tasks and pipeline bubbles, and supports further performance analysis.| -                                |

- Operator Tuning

  MindStudio Insight provides the instruction pipeline view, operator source code view, and operator runtime load analysis view to visualize the key performance metrics of operators running on the Ascend AI Processor, helping developers quickly locate software and hardware performance bottlenecks and improve operator performance analysis efficiency. For details, see [Operator Tuning](./docs/en/user_guide/operator_tuning.md).

  | Function Interface          | Description                                                        | Remarks                                    |
    | ------------------ | ------------------------------------------------------------ | ---------------------------------------- |
    | Timeline| Displays the running status of instructions on the Ascend AI Processor in a timeline view, displays the overall running status based on the scheduling process, and allows users to view instruction details and search for instructions.| -                                        |
    | Source    | Displays the operator instruction heatmap, and allows developers to view the mapping between the operator source code and instruction sets as well as the time consumption.| BIN files of operator profiling collected by msProf are supported.|
    | Details   | Displays the basic operator information, compute workload analysis, and memory workload analysis, as well as the analysis results in charts and data panes.| BIN files of operator profiling collected by msProf are supported.|
    | Cache     | Displays the L2 cache access of kernel functions in user programs, helping users optimize the cache hit rate.| BIN files of operator profiling collected by msProf are supported.|

- Serving tuning

  MindStudio Insight displays the end-to-end request execution in the timeline view, showing the duration of the request in each key phase and the status of the request. This helps users quickly identify service performance bottlenecks and adjust the profiling policy accordingly. For details, see [Serving Tuning](./docs/en/user_guide/service_optimization.md).

  | Function Interface          | Description                                                        | Scenario Description                               |
  | ------------------ | ------------------------------------------------------------ | --------------------------------------- |
  | Timeline| Displays the end-to-end request execution status in a timeline view, helping users intuitively view the duration of the request in each key phase and the current request status.| JSON files of trace data of inference service requests are supported.|
  | Curve   | Displays the end-to-end performance of the inference service process in a curve and a data details table.| The **profiler.db** file is supported.                  |

- Memory Tuning

  MindStudio Insight displays the detailed memory allocation on the device in graphics, and marks the usage details of various memory allocations based on the Python call stack and custom dotting tags to locate and optimize memory problems. For details, see [Memory Tuning](./docs/en/user_guide/memory_tuning.md).

  | Function Interface         | Description                                                        | Scenario Description                                     |
  | ----------------- | ------------------------------------------------------------ | --------------------------------------------- |
  | Leaks| Displays call stack diagrams, curve block charts, and memory breakdown diagrams to visualize memory usage, helping developers analyze and locate memory issues and effectively reduce diagnosis time.| DB memory result files collected by msLeaks are supported.|

## 📝 Related Information

- [FAQ](./docs/en/user_guide/FAQ.md)
- [Contribution Guide](CONTRIBUTING.md)
- License<br>
  For details about the license for using MindStudio Insight, see [LICENSE](./License).<br>Documents in the `docs` directory of MindStudio Insight are licensed under CC-BY 4.0. For details, see [DOC LICENSE](./docs/LICENSE).
- [Security and Disclaimer](./DISCLAIMER.md)

## 💬 Suggestions and Feedback

You are welcome to contribute to the community. If you have any questions or suggestions, please submit an [issue](https://gitcode.com/Ascend/msinsight/issues). We will reply as soon as possible. Thank you for your support.

## 🤝 Acknowledgments

MindStudio Insight is jointly developed by the following Huawei departments:

- Computing Product Line

Thank you to everyone in the community for your PRs. We warmly welcome contributions to MindStudio Insight!

## About the Team

The Huawei MindStudio full-pipeline development toolchain team is dedicated to providing an end-to-end solution for building Ascend AI applications, accelerating the processes of training, inference, and operator development. You can learn more about the Huawei MindStudio team through the following channels:
<div style="display: flex; align-items: center; gap: 10px;">
    <span>MindStudio official account:</span>
    <img width="100" src="./docs/zh/user_guide/figures/readme/officialAccount.png" />
    <span style="margin-left: 20px;">Ascend Assistant: </span>
    <a href="https://gitcode.com/Ascend/msinsight/blob/master/docs/zh/user_guide/figures/readme/xiaozhushou.png">
        <img src="https://camo.githubusercontent.com/22bbaa8aaa1bd0d664b5374d133c565213636ae50831af284ef901724e420f8f/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f5765436861742d3037433136303f7374796c653d666f722d7468652d6261646765266c6f676f3d776563686174266c6f676f436f6c6f723d7768697465" data-canonical-src="./docs/zh/user_guide/figures/readme/xiaozhushou.png" style="max-width: 100%;">
    </a>
    <span style="margin-left: 20px;">Ascend Forum: </span>
    <a href="https://www.hiascend.com/forum/" rel="nofollow">
        <img src="https://camo.githubusercontent.com/dd0b7ef70793ab93ce46688c049386e0755a18faab780e519df5d7f61153655e/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f576562736974652d2532333165333766663f7374796c653d666f722d7468652d6261646765266c6f676f3d6279746564616e6365266c6f676f436f6c6f723d7768697465" data-canonical-src="https://img.shields.io/badge/Website-%231e37ff?style=for-the-badge&amp;logo=bytedance&amp;logoColor=white" style="max-width: 100%;">
    </a>
</div>
Send "communication group" to the official account to obtain the QR code of the technical communication group.
