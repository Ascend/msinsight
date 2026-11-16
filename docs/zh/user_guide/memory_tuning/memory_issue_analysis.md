# **内存问题分析案例**

## 概述

在昇腾全栈开发活动中，内存问题较为常见，但是由于内存问题的软件栈层次复杂（包括但不限于操作系统的驱动和运行时库、CANN、MindSpore/PyTorch\_NPU、模型训练和模型推理等），导致内存问题的定位和解决往往较为困难。目前典型的内存问题分类可参见[**表 1**  内存问题分类](#内存问题分类)。

本文介绍通过MindStudio Insight工具定位内存问题的方法。

**表 1**  内存问题分类<a id="内存问题分类"></a>

|问题类别|问题现象|场景|
|--|--|--|
|内存踩踏|出现精度异常或出现NaN，通常出现在Device上。|训练、推理、算子开发|
|内存使用过多|内存使用过多，通常与以下两种情况有关：<br> - 泄漏或OOM（Out of Memory，内存溢出）：Host侧内存监测持续增长，甚至OOM;或者Device侧内存使用量持续增长，甚至OOM。<br> - 与预期或基线相差大：实际采集的内存使用数据远超预期或基线数据，差值会达到GB量级，通常出现在Device侧。|训练、推理|

**定位流程**

针对Device侧内存使用过多或OOM，问题分析流程如下：

1. 通过性能调优工具采集性能数据，并导入MindStudio Insight；
2. 查看内存（Memory）界面中“内存分析”区域的内存曲线图、算子或组件内存申请/释放详情，进行基础定界，明确异常范围、Step或算子；
3. 使用内存工具（msMemScope）采集对应异常范围的内存详情与内存拆解数据，导入MindStudio Insight；
4. 查看内存详情（MemScope）界面，结合“调用栈火焰图”、“内存块生命周期图”、“内存详情表”进行内存占用拆解分析。

## 前期准备

**准备软件**

- 下载MindStudio Insight工具并安装，请参见[MindStudio Insight安装指南](../../install_guide/mindstudio_insight_install_guide.md)。
- 安装msMemScope工具，安装操作请参见[msMemScope工具安装指南](https://gitcode.com/Ascend/msmemscope/blob/master/docs/zh/install%5Fguide/install%5Fguide.md)。

**准备数据**

以下采集的数据为内存泄漏的数据。

1. 使用msMemScope工具执行如下命令，在每个Step中申请一个大小为4 x 10MB的Tensor，并追加到全局变量leak\_mem\_list列表中（不会随train\_one\_step释放），采集3个Step的Python Trace数据。

    ```shell
    msmemscope --level=0,1 --events=alloc,free,access,launch --analysis=decompose --data-format=db python test.py
    ```

    其中test.py的示例代码如下：

    ```python
    import torch
    import torch_npu
    from torchvision.models import resnet50
    import msmemscope
    import msmemscope.describe as describe
    leak_mem_list = []
    def train_one_step(model, optimizer, loss_fn, device):
        # 对代码块做标记，代码块内所有内存申请事件的owner属性都会打上标签leaks_mem
        describe.describer(owner="leaks_mem").__enter__()
        # 内存泄漏代码段
        leak_mem_list.append(torch.randn(1024 * 1024 * 10, dtype=torch.float32).to(device))
        # 结束标记
        describe.describer(owner="leaks_mem").__exit__(None, None, None)
        # 单次训练代码段
        inputs = torch.randn(1, 3, 224, 224).to(device)
        labels = torch.rand(1, 10).to(device)
        pred = model(inputs)
        loss_fn(pred, labels).backward()
        optimizer.step()
        optimizer.zero_grad()
    def train(model, optimizer, loss_fn, device, steps=1):
        for i in range(steps):
            train_one_step(model, optimizer, loss_fn, device)
    device = torch.device("npu:0")
    torch.npu.set_device(device)  # 设置device
    model = resnet50(pretrained=False, num_classes=10).to(device)  # 加载模型
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-2)  # 定义优化器
    loss_fn = torch.nn.CrossEntropyLoss()  # 定义损失函数

    # 开启采集python函数调用数据
    msmemscope.tracer.start()
    train(model, optimizer, loss_fn, device, steps=3)  # 开始训练

    # 停止采集python函数调用数据
    msmemscope.tracer.stop()
    ```

2. 采集完成后，输出db格式的文件。
3. 将文件下载至本地保存。

## 内存分析

**导入数据**

1. 打开MindStudio Insight工具，单击左侧导航栏的“导入数据”。
2. 在弹出的“文件资源管理器”弹框中，选择需要导入的db格式的文件。
3. 导入成功后，显示“内存详情”界面。

**内存分析**

1. 打开“内存详情”界面，查看“调用栈火焰图”和“内存块生命周期图”。
2. 单击鼠标左键框选“内存块生命周期图”中的Step2区域，松开鼠标左键，放大Step2区域。

    可以从[**图 1**  未释放的内存块](#未释放的内存块)中看出，在Step2结束时，仍存在一个未释放的内存块。

    **图 1**  未释放的内存块<a id="未释放的内存块"></a>

    ![](../figures/memory_tuning/unreleased_memory_blocks_1.png "未释放的内存块")

3. 查看“调用栈火焰图”，发现该内存块来自于一个Tensor对象，在前向传播开始前即已申请，如[**图 2**  Tensor对象](#Tensor对象)所示。

    **图 2**  Tensor对象<a id="Tensor对象"></a>

    ![](../figures/memory_tuning/tensor_object_1.png "Tensor对象")

4. 对照leaks\_mem标记的代码段，查看“内存详情拆解图”，发现leaks\_mem标记的代码段存在明显的增长，从Step 1开始leaks\_mem标签内存占用首次出现为40MB，如[**图 3**  查看Step 1的内存占用](#查看Step-1的内存占用)所示。

    **图 3**  查看Step 1的内存占用<a id="查看Step-1的内存占用"></a>

    ![](../figures/memory_tuning/view_step1_memory_usage_1.png "查看Step-1的内存占用")

    如[**图 4**  查看Step 2的内存占用](#查看Step-2的内存占用)所示，Step 2中leaks\_mem标签内存占用从40MB增长到了80MB。

    **图 4**  查看Step 2的内存占用<a id="查看Step-2的内存占用"></a>

    ![](../figures/memory_tuning/view_step2_memory_usage_1.png "查看Step-2的内存占用")

    如[**图 5**  查看Step 3的内存占用](#查看Step-3的内存占用)所示，Step 3中leaks\_mem标签内存占用从80MB增长到了120MB。

    **图 5**  查看Step 3的内存占用<a id="查看Step-3的内存占用"></a>

    ![](../figures/memory_tuning/view_step3_memory_usage_1.png "查看Step-3的内存占用")
