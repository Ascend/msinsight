# **🚀 Quick Start (System Tuning)**

MindStudio Insight allows you to import the model system profile data collected by [msProf](https://gitcode.com/Ascend/msprof/blob/26.0.0/docs/en/getting_started/quick_start.md) on the Ascend AI Processors. Users can quickly identify software and hardware performance bottlenecks for models based on the displayed key performance indicators, enabling efficient system performance tuning.

## Environment Setup

When a foundation model is used in a cluster, slow and fast cards may exist, which reduces the model performance. msProf can collect and parse the running data of AI models and the system data of Ascend AI Processors during training and inference.

Assume that you have used msProf to collect the system profile data of a model running on a dual-node 16-device cluster.

System data: [Click here to download](https://gitcode.com/zhangruoyu2/msinsight-quick-start-demo/blob/main/system)

<details>
<summary>📁Data directory structure</summary>

```tex
└─MultiProfLevel2MemoryUB_db
    ├─cluster_analysis_output
    ├─node1_2166651_20240619060505060_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505099_GNIJBPBEBIHIHCKB
    ├─node1_2166652_20240619060505059_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505221_GOGOEMKAROGRMIMC
    ├─node1_2166653_20240619060505061_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505106_OBPMMFGEPENJQFGB
    ├─node1_2166654_20240619060505060_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505226_QDMBAQFOGNGMDQKA
    ├─node1_2166655_20240619060505059_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505102_CAMQMCOANGFPNHKC
    ├─node1_2166656_20240619060505059_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505221_HEKHQMKAREGIKAQB
    ├─node1_2166657_20240619060505060_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505169_QQNFRMQFEEKHCIFA
    ├─node1_2166658_20240619060505060_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619140505196_CHIRLGBCBGMLEFJC
    ├─ubuntu2204_1660963_20240619060440181_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440316_NDJQFQRGIMPECACC
    ├─ubuntu2204_1660964_20240619060440179_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440323_JQBHAHKEDBPDBBDC
    ├─ubuntu2204_1660965_20240619060440181_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440310_CGDJCKDFAOCOKNMB
    ├─ubuntu2204_1660966_20240619060440181_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440326_QKMBONEJDLBAHCOA
    ├─ubuntu2204_1660970_20240619060440179_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440334_AGCKEFNHPCNEHKHB
    ├─ubuntu2204_1660971_20240619060440180_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440319_OAEDHALJOJOJKECC
    ├─ubuntu2204_1660972_20240619060440181_ascend_pt
    │  ├─ASCEND_PROFILER_OUTPUT
    │  ├─FRAMEWORK
    │  └─PROF_000001_20240619060440320_RAGMCOQPGDMOJECA
    └─ubuntu2204_1660973_20240619060440181_ascend_pt
        ├─ASCEND_PROFILER_OUTPUT
        ├─FRAMEWORK
        └─PROF_000001_20240619060440316_MONIINKACEIFDCRA
```

</details>

## Procedure

### 1. Analyzing the Summary Tab Page

Import the `MultiProfLevel2MemoryUB_db` folder and switch to the **Summary** tab page.

![summary overview](../figures/quick_start/system_quick_start_summary_overview.png)

In the computing/communication overview, significant free time is observed for the ranks 8 and 15, indicating underutilized resources and room for performance optimization.

> Preliminary findings: The performance can be optimized.

### 2. Analyzing the Communication Tab Page

Switch to the **Communication** tab page and select the **Communication Duration Analysis** option.

![communication duration analysis](../figures/quick_start/system_quick_start_comm_duration_analysis.png)

Observing the communication operator thumbnail, the first operator on Rank 15 has the shortest duration, indicating that the Rank 15 configuration is the performance bottleneck and needs further analysis.

>[!NOTE]  
> The data uses the parallel policy. Therefore, the data of each device needs to be synchronized through communication at a specified interval.
> Long communication time indicates a stall while waiting for other ranks to be ready for data synchronization. Short communication time suggests that the rank is still performing other operations and is not yet ready to sync.
> Follow-up findings: The Rank 15 configuration is the performance bottleneck and needs to be analyzed in detail.

### 3. Analyzing the Timeline Tab Page

Right-click the minimum communication operator on Rank 15 in the communication operator thumbnail, select **Find in Timeline**, and then zoom in appropriately to observe Rank 15's activities prior to the communication.

![timeline slow question](../figures/quick_start/system_quick_start_timeline_slow_question.gif)

Select the **Overlap Analysis** unit and view Rank 15's activities.

>[!NOTE]  
>The Computing sub-unit is a projection of the Ascend Hardware unit above, indicating when the rank is actively computing. The Communication sub-unit is a projection of the Communication unit, indicating when the rank is engaged in communication. The Free sub-unit indicates when the rank is free.

![timeline unit selected list](../figures/quick_start/system_quick_start_selected_unit_list.png)

Observing the selected slice list, the free duration is approximately three times the compute duration. Free durations are typically caused by CPU-bound user code or host thread preemption. More data is required for further analysis.

> Conclusion: The performance bottleneck is due to high rank free time. Next steps include reviewing user code and collecting host-side data to further diagnose the root cause.
