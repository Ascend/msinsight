# 强化学习（RL）<a id="rl"></a>

## 功能说明

**简介**

强化学习（RL）界面提供了强化学习过程中各阶段流水图的可视化展示，使能开发者能够全面了解运行情况，轻松定位问题，并进行深入分析与优化。

**注意事项**

- 仅当导入使用mstx打点采集到的控制流数据时，才会展示任务执行时间线。mstx打点采集方式请参见《Ascend PyTorch调优工具》文档中的“[采集并解析性能数据（torch_npu.profiler.profile）](https://gitcode.com/Ascend/pytorch/blob/v2.7.1-26.1.0/docs/zh/ascend_pytorch_profiler/ascend_pytorch_profiler_user_guide.md#%E9%87%87%E9%9B%86%E5%B9%B6%E8%A7%A3%E6%9E%90%E6%80%A7%E8%83%BD%E6%95%B0%E6%8D%AEtorch_npuprofilerprofile)”章节内容。
- 当导入verl（Volcano Engine Reinforcement Learning for LLMs）和MindSpeed框架的性能数据时，需分别导入各自的性能数据文件夹，不支持将两种数据合并放在同一文件夹中导入。

## 界面介绍

强化学习界面由参数配置栏（区域一）和任务执行时间线（区域二）组成，如[**图 1** 强化学习界面](#强化学习界面)所示。

**图 1** 强化学习界面<a id="强化学习界面"></a>

![](../figures/system_tuning/reinforcement_learning_interface_1.png "强化学习界面")

- 区域一：参数配置栏，自动识别并显示导入数据的“框架”和“算法”；当导入的数据大于16卡时，强化学习界面数据可能展示不全，可单击“刷新”解析所有数据，刷新任务执行时间线。
- 区域二：任务执行时间线，展示的是各卡上每个任务的执行时间，横坐标为时间轴，纵坐标为各设备对应的RankID，不同的颜色代表不同的任务，其中蓝色色块中提供Forward和Backward micro batch标记展示，帮助定位训练阶段的细粒度性能问题。

    通过任务执行时间线图形右侧和下方的滑动框可以缩放、移动时间线，也可以通过Ctrl + 鼠标滚轮进行缩放。
