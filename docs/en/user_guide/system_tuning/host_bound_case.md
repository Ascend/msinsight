# Host Bound Issue Analysis Cases

<a id="host-bound-analysis-case"></a>

In a foundation model system, the CPU is responsible for task delivery, and the NPU is responsible for task execution. In actual inference or training processes, host bound issues are common. To address the host bound issues, it is typically necessary to analyze CPU scheduling problems by collecting Linux kernel ftrace data. However, there is currently a lack of tools that can uniformly visualize Linux kernel ftrace data alongside profile data, making it difficult to analyze host bound issues.

MindStudio Insight provides a complete set of data collection and conversion scripts. It can collect Linux kernel ftrace data and convert it into the supported JSON file format. Additionally, this tool allows you to import both the converted Linux kernel ftrace data and profile data, displaying them on the same interface for joint analysis. This improves the efficiency of locating host bound issues.

For details about how to analyze host bound issues, see [Host Bound Issue Analysis Cases](https://gitcode.com/Ascend/msinsight/blob/master/scripts/ftrace_tools/ReadMe.md).
