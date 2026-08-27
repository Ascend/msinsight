# Quick Start (Operator Tuning)

<!-- md-trans-meta sourceCommit=4d31aa7808cfa63b1e9a65d9252c982b334ca6f5 translatedAt=2026-08-12T11:44:04.336Z pushedAt=2026-08-12T11:57:31.117Z -->

MindStudio Insight supports importing operator profile data collected by the [msOpProf](https://gitcode.com/Ascend/msopprof) tool and running on Ascend AI Processors. Based on the displayed key operator performance indicators, users can quickly identify software and hardware performance bottlenecks of operators and perform operator tuning.

This document uses the `matmul_leakyrelu` operator as an example to demonstrate how to identify performance anomalies from on-board data and then locate the user code lines that may require optimization through simulation data.

## 1. Scope and Prerequisites

### 1.1 Scope

This document is intended for developers who want to quickly experience the operator tuning capabilities of MindStudio Insight. It focuses on demonstrating the following process:

1. Import the on-board data collected by msOpProf to view the overall operator duration and pipeline ratio.

2. Import the simulation data collected by msOpProf to view Timeline and Source information.

3. Identify user code locations that may cause performance bottlenecks based on hotspot behavior.

>[!NOTE]
>
> This document uses prepared sample data to demonstrate the analysis path, without elaborating on msOpProf data collection commands. If you need to collect real operator data, refer to the data description in [MindStudio Insight Operator Tuning](../user_guide/operator_tuning.md) and the msOpProf tool documentation.

### 1.2 Pre-Start Check

| Check Item | Requirement |
| --- | --- |
| Tool installation | MindStudio Insight has been installed. For the installation method, see [MindStudio Insight Installation Guide](../install_guide/mindstudio_insight_install_guide.md). |
| Version compatibility | The versions of MindStudio Insight, CANN, and the collection tool must be compatible. For version relationships, see [Release Notes](../release_notes/release_notes.md). |
| Sample data | The operator sample data provided in this document has been downloaded and is accessible locally. |
| Data source | The sample data is collected by [msOpProf](https://gitcode.com/Ascend/msopprof) and includes on-board data and simulation data. |
| Applicable scenario | Suitable for getting started with Ascend operator performance analysis, especially for learning the positioning relationships among the Details, Timeline, and Source tabs. |

### 1.3 Sample Data

Operator data: [Click to download](https://gitcode.com/zhangruoyu2/msinsight-quick-start-demo/blob/main/operator)

After downloading, verify that the directory contains the following two types of data:

```text
├─msprof-op                  # On-board data for inspecting actual hardware execution latency and basic performance metrics
│  ├─core_inter_load
│  ├─details
│  ├─ratio
│  ├─roofline
│  ├─source
│  └─timeline
└─msprof-op-simulator         # Simulation data for inspecting fine‑grained timelines and source‑level hotspots
```

The following sections will import them respectively:

- `msprof-op/details/visualize_data.bin`: View on-board data.

- `msprof-op-simulator/visualize_data.bin`: View the simulation data.

### 1.4 Terminology

| Term | Description |
| --- | --- |
| On-board data | Data collected after running an operator on real Ascend hardware, suitable for observing actual latency, pipeline ratio, and hardware utilization. |
| Simulation data | Data collected through the simulator, suitable for observing finer-grained instruction pipelines, code hotspots, and behavior distribution. |
| Details | Used to view summary metrics such as basic operator information, memory load, and pipeline ratio. |
| Timeline | Used to view the behavior and pipeline status during operator execution in chronological order. |
| Source | Used to correlate hotspot behavior with user source code locations. |
| Scalar | Scalar computing unit. Excessively high Scalar activity usually indicates a large number of scalar control or configuration operations. |
| Cube | Matrix computing unit. For matrix-type operators, the primary latency is typically expected to be concentrated on Cube computations. |

## 2. Procedure

### 2.1 Viewing On-Board Data

**Operation:** Import the `msprof-op/details/visualize_data.bin` on-board data file and switch to the **Details** tab.

**Observation objective:** First check the overall operator duration to determine whether it significantly deviates from expectations.

![base info question](./figures/quick_start/operator_quick_start_base_info.png)

In the basic information section, the operator duration is found to be approximately **90+ μs**.

> For the `matmul_leakyrelu` operator, performance mainly depends on the matrix multiplication part, while the computing load of LeakyReLU is relatively small.

Based on the prior baseline of this sample, when computing a small FP16 matrix (`1024 × 1024 × 1024`) on the Ascend 910, the expected time of the `matmul_leakyrelu` operator typically falls within the range of **16–30 μs**. The current sample takes approximately **90+ μs**, which is significantly higher than expected. Therefore, it can be preliminarily determined that there is room for optimization in this operator.

>[!NOTE]
>
> The expected duration is affected by the hardware model, CANN/msOpProf version, operator implementation, and input scale. The values in this document are used to illustrate the analysis method. In actual services, the judgment should be based on the baseline or optimization target in the same environment.

**Next step:** Continue to view the pipeline ratio to determine whether the time consumption is concentrated on unexpected compute units.

![pipe cube scalar high](./figures/quick_start/operator_quick_start_pipe_cube_scalar.png)

In the memory load analysis section, examine the pipeline status. It is observed that Scalar activity is relatively high in the Cube pipeline. For operators dominated by matrix multiplication, the primary computing time is typically expected to concentrate on Cube-related computations. If the Scalar behavior ratio is significantly high, it indicates that the operator may contain a considerable amount of scalar configuration, control, or object initialization operations, and further investigation is needed to pinpoint the specific source.

> Preliminary conclusion: The overall operator duration is higher than expected, and the Scalar behavior ratio is relatively high. It is recommended to continue locating the specific code position through simulation data.

### 2.2 Viewing Simulation Data

**Operation:** Import the `msprof-op-simulator/visualize_data.bin` simulation data file and switch to the **Timeline** tab.

**Observation objective:** View the behavior distribution in the Scalar unit to confirm whether the Scalar activity issue observed in the on-board data can be located on the timeline.

![timeline scalar unit](./figures/quick_start/operator_quick_start_timeline_scalar_unit.png)

On the **Timeline** tab, view the behaviors during the operator execution process. It can be seen that there are many behaviors in the Scalar unit, which is consistent with the preliminary conclusion of "high Scalar activity" from the on-board data.

**Operation:** Use the mouse to select a range in the Scalar unit, pick the behavior with the highest occurrence count or the most significant time consumption, and check which code segment causes it.

**Judgment basis:** If a certain type of Scalar behavior occurs frequently or forms dense segments on the timeline, it usually indicates that the behavior may contribute significantly to the overall time consumption and should be prioritized for locating its source code origin.

![find max behavior](./figures/quick_start/operator_quick_start_find_max_behavior.gif)

After selecting the target behavior, view the associated source code location in the details area.

![detail code position](./figures/quick_start/operator_quick_start_detail_code_position.png)

You can locate the user code position from the details area. An example path is as follows:

```text
/path/to/samples/operator/ascendc/0_introduction/13_matmulleakyrelu_kernellaunch/MatmulLeakyReluInvocationAsync/matmul_leakyrelu_custom.cpp:206
```

>[!NOTE]
>
> The actual path varies depending on where the sample code is stored. During analysis, prioritize the user source file and line number in the path rather than the toolchain internal file paths.

**Operation:** Switch to the **Source** tab and open the user code found earlier.

![source code position](./figures/quick_start/operator_quick_start_source_code.png)

```c
206     REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), matmulLeakyKernel.matmulObj, &matmulLeakyKernel.tiling);
```

`REGIST_MATMUL_OBJ` is a macro used to initialize the Matmul object and set Tiling parameters. According to Ascend official documentation, this macro internally executes a series of scalar operations to configure the Cube compute unit. Therefore, it may be a significant source of the high Scalar behavior in this example.

> Conclusion: By following the path of "identifying anomalies in on-board data → viewing the **Timeline** in simulation data → locating source code in the **Source** Tab," you can pinpoint the lines of code that may require optimization. The next step is for the operator development engineer to evaluate, based on the operator implementation, whether repeated initialization can be reduced, the Tiling design can be optimized, or the usage of Matmul objects can be adjusted.

## 3. FAQs and Troubleshooting Entry Points

| Symptom | Suggested Action |
| --- | --- |
| No data is displayed after importing `visualize_data.bin` | First confirm that the correct `visualize_data.bin` file from the on-board data or simulation data has been imported, and then refer to the data import-related issues in [FAQs](../support/faq.md). |
| The page display does not exactly match the screenshots in this document | The interface fields may vary slightly across different versions of MindStudio Insight, CANN, or msOpProf. Refer to the key metrics and source code location information in the **Details**, **Timeline**, and **Source** tabs as the authoritative reference. |
| Unable to locate the source code | Check whether source code information was included during msOpProf collection, or refer to the data description in [MindStudio Insight Operator Tuning](../user_guide/operator_tuning.md). |
| Meaning of Timeline units | Refer to [Timeline Unit Introduction](../best_practices/Timeline_Common_Lanes_and_Interface.md). |

## 4. More Information

- To learn about data import, opening views, and basic operations, see [MindStudio Insight Basic Operations](../user_guide/basic_operations.md).

- To gain in-depth knowledge of operator tuning capabilities, see [MindStudio Insight Operator Tuning](../user_guide/operator_tuning.md).

- To learn about common Timeline units and interactions, see [Timeline Unit Introduction](../best_practices/Timeline_Common_Lanes_and_Interface.md).

- If you encounter import or UI issues, see [FAQs](../support/faq.md).
