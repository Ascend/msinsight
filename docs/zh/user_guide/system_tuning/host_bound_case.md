# Host Bound问题分析案例

<a id="host-bound-analysis-case"></a>

在大模型系统中，CPU主要负责任务下发，NPU负责任务执行。在实际的推理或训练过程中，Host Bound问题较为常见。要解决Host Bound问题，通常需要通过收集Linux Kernel ftrace数据分析CPU调度问题，但目前缺乏将Linux Kernel ftrace数据与Profiling的数据统一展示的工具，导致Host Bound问题分析存在困难。

MindStudio Insight支持两种Ftrace输入：MindStudio Insight工具提供的一套完整的数据采集和转换脚本采集并转换的独立DB/JSON，以及msprof profiling DB内置的Ftrace。前者与对应的profiling数据联合导入，后者随主DB自动识别；两者均可结合CPU Scheduling、Process Scheduling和profiling视图分析Host Bound问题。

Host Bound问题分析的具体步骤请参见《[Host Bound问题分析案例](https://gitcode.com/Ascend/msinsight/blob/master/scripts/ftrace_tools/ReadMe.zh-CN.md)》。
