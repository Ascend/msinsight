<h1 align="center">MindStudio Insight</h1>
<div align="center">
  <img src="./modules/framework/public/favicon.ico" width="200" alt="MindStudio Insight Logo">
  <p>🚀 <b>昇腾 AI 全流程可视化调优利器</b></p>
  
  [![Ask DeepWiki](https://badgen.net/badge/Ask/DeepWiki/blue)](https://deepwiki.com/qianxiaoxixixi/MsInsightForEveryOne/ ) [![Ask ZRead](https://badgen.net/badge/Ask/ZRead/orange)](https://zread.ai/qianxiaoxixixi/MsInsightForEveryOne) [![doc](https://badgen.net/badge/doc/readthedocs/green)](https://msinsight.readthedocs.io/zh-cn/latest/)
  [![License](https://badgen.net/badge/License/MulanPSL-2.0/blue)](./License) [![Version](https://badgen.net/badge/Version/26.0.0/green)](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.0.0.B120_0012) [![Ascend](https://img.shields.io/badge/Hardware-Ascend-orange.svg)](https://www.hiascend.com/)
</div>

## ✨ 最新动态

- **[2026.4.29]** 🎉 **MindStudio Insight 26.0.0 版本上线！** 聚焦**hostbound**问题定位跟**RL**性能分析优化,snapshot内存大数据分析，开启昇腾 AI 调优新体验。

## ℹ️ 简介

**MindStudio Insight** 是专为昇腾 AI 开发者打造的深度可视化调优分析工具。它通过可视化手段呈现真实的软硬件运行数据，帮助开发者在天级时间内精准定位并解决性能瓶颈。

### 核心价值

- **全场景覆盖：** 支持系统调优、算子调优、服务化调优及内存调优。
- **超大规模支持：** 轻松应对百卡、千卡级集群分析，支持高达 **20GB+** 的性能数据处理。
- **极简操作：** 自动遍历 Profiling 数据，无需手动合并文件，即插即用。

### Demo

<div style="
  padding: 4px 0 2px 0;            /* 极窄上下 */
  font-weight: 600;                 半粗体
  color: #1a2639;
  letter-spacing: 1px;           
">
  系统调优
</div>
<div style="padding: 0;">
  <img style="width: 100%; height: auto; display: block;" alt="大文件导入" src="./assets/demo-system.gif" />
</div>

<details>
<summary style="font-size: 24px; font-weight: bold;">🗺️ 目录结构</summary>

```tex
├── build                              # 构建脚本 
├── docs                               # 项目文档介绍 
├── e2e                                # 测试用例
├── modules                            # 模块目录
│   ├── build                          # 构建脚本
│   ├── cluster                        # 概览、通信模块
│   ├── compute                        # 算子调优模块
│   ├── framework                      # 前端主框架模块
│   ├── leaks                          # 内存泄露模块
│   ├── lib                            # 公共目录
│   ├── memory                         # 内存模块
│   ├── operator                       # 算子模块
│   ├── reinforcement-learning         # 强化学习模块
│   ├── statistic                      # 服务化调优模块
│   ├── timeline                       # 时间线模块
├── platform                           # 底座目录 
├── plugins                            # 插件目录
├── scripts                            # 脚本目录 
├── server                             # 后端服务模块
│   ├── build                          # 构建脚本
│   ├── cmake                          # 开源软件构建脚本
│   ├── src                      
│   │   ├── channel                    # 网络通讯
│   │   ├── defs                       # 全局定义
│   │   ├── entry                       # 编译模块
│   │   │   ├── server       
│   │   │      ├── bin                 # server模块
│   │   ├── protocol                   # 消息定义
│   │   ├── module       
│   │   │   ├── base                   # 模块共用基类 
│   │   │   ├── global                 # 全局消息
│   │   │   ├── timeline               # timeline消息处理
│   │   │   │   ├── core               # 核心处理逻辑
│   │   │   │   ├── handler            # 消息处理
│   │   │   │   ├──protocol            # 消息格式转换
│   │   │   ├── ...      
│   │   ├── server                     # server服务
│   │   ├── utils                      # 工具类
│   ├── third_party                    # 开源软件 
```

</details>

## 🔖 版本说明

| 发布版本 | 发布时间        | 发布Tag       | 兼容性说明    |
| ------- | --------------- | ------------- | ------------- |
| 26.0.0 | 2026/04/29  | tag_MindStudio_26.0.0.B120_0012 | 兼容昇腾CANN 9.0.0及以前版本。请参考[CANN快速安装](https://www.hiascend.com/cann/download?version=683&filter=)获取CANN安装包。 |
| 26.0.0-alpha.1 | 2026/02/04  | tag_MindStudio_26.0.0-alpha.1 | 兼容昇腾CANN 8.5.0及以前版本。请参考[CANN快速安装](https://www.hiascend.com/cann/download)获取CANN安装包。 |
| 26.0.0-beta.1 | 2026/04/08  | tag_MindStudio_26.0.T2.B100_001 | 兼容昇腾CANN 8.5.0及以前版本。请参考[CANN快速安装](https://www.hiascend.com/cann/download)获取CANN安装包。 |

**[查看完整版本发布说明](./docs/zh/release_notes.md)**

## 📦 安装

MindStudio Insight工具支持在Windows、Linux，以及macOS系统中安装使用，并且支持通过插件方式安装，具体安装操作可参见[MindStudio Insight安装指南](./docs/zh/user_guide/mindstudio_insight_install_guide.md)。

## 🚀 快速入门

- [系统调优篇](./docs/zh/user_guide/quick_start/system_tuning_quick_start.md)：学习如何使用概览、通信、时间线页签分析模型系统性能
- [算子调优篇](./docs/zh/user_guide/quick_start/operator_tuning_quick_start.md)：学习如何使用详情、时间线、源码页签分析算子性能

---

## ⚠️ 工具限制与注意事项

MindStudio Insight工具支持导入并展示多种格式的性能数据文件，并对文件规格给出了指导性建议和限制要求。

| 文件类型 | 指导建议                                            | 规格限制               |
| ----------- | --------------------------------------------- | ---------------------- |
| json文件 | 建议单文件大小不超过1GB，多个文件总大小不超过20GB。          | 单文件大小不超过20GB。 |
| bin文件  | 建议单文件大小不超过500MB。                                 | 单文件大小不超过20GB。 |
| db文件   | - 系统调优：建议单文件大小不超过1GB。<br> - 服务化调优：建议单文件大小不超过1GB。 | - 系统调优：单文件大小不超过20GB。<br> - 服务化调优：单文件大小不超过10GB。 |
| csv文件  | csv格式的文件存在于text数据中，建议单文件大小不超过500MB。     | 单文件大小不超过2GB。  |

## 📘 基础操作

介绍MindStudio Insight工具的基础设置，数据导入，快捷键使用等基础操作。具体请参见[MindStudio Insight基础操作](./docs/zh/user_guide/basic_operations.md)。

## ⚙️ 特性介绍

MindStudio Insight工具支持系统调优、算子调优、服务化调优和内存调优，可视化呈现数据情况，助力开发者快速完成性能调优。

- 系统调优

  MindStudio Insight提供时间线视图、内存、算子耗时、通信瓶颈分析等功能，帮助开发者快速定位模型性能瓶颈，进行深度调优。具体内容请参见[系统调优](./docs/zh/user_guide/system_tuning.md)。

  | 功能界面              | 介绍                                                         | 场景说明                         |
  | --------------------- | ------------------------------------------------------------ | -------------------------------- |
  | 时间线（Timeline）    | 以时间线视图方式为用户提供全流程在线推理/训练过程中的运行情况，并按照调度流程来呈现整体的运行状况，支持集群时间线（Timeline）展示、系统视图详情查看等功能。 | -                                |
  | 内存（Memory）        | 提供采集过程中内存信息的可视化呈现。通过算子内存折线图直观清晰了解算子内存趋势。 | -                                |
  | 算子（Operator）      | 提供算子耗时统计和分析。                                     | -                                |
  | 概览（Summary）       | 展示计算算子和通信算子的耗时分析，并以柱状图、折线图以及数据窗格等呈现方式显示分析结果。 | 支持PyTorch集群场景。 |
  | 通信（Communication） | 展示集群中全网链路性能以及所有节点的通信性能，通过集群通信与计算重叠时间的分析可以找出集群训练中的慢主机或慢节点。 | 支持PyTorch集群场景。 |
  | 强化学习（RL）        | 基于采集数据进行高层次抽象，可视化展示控制流的时序关系，帮助快速定位耗时任务与空泡，并支持进一步的性能分析。 | -                                |

- 算子调优

  MindStudio Insight提供指令流水视图、算子源码视图以及算子运行负载分析视图，直观地将运行在昇腾AI处理器上的算子的关键性能指标进行可视化呈现，帮助用户快速定位算子的软硬件性能瓶颈，提升算子性能分析的效率。具体内容请参见[算子调优](./docs/zh/user_guide/operator_tuning.md)。

  | 功能界面           | 介绍                                                         | 备注                                     |
    | ------------------ | ------------------------------------------------------------ | ---------------------------------------- |
    | 时间线（Timeline） | 以时间线视图方式为用户提供指令在昇腾处理器上的运行情况，并按照调度流程来呈现整体的运行状况，支持查看指令详情、搜索指令等功能。 | -                                        |
    | 源码（Source）     | 展示算子指令热点图，支持查看算子源码与指令集的映射关系和耗时情况。 | 支持msprof采集的算子Profiling的bin文件。 |
    | 详情（Details）    | 展示算子基础信息、计算负载分析和内存负载分析，并以图形和数据窗格呈现方式展示分析结果。 | 支持msprof采集的算子Profiling的bin文件。 |
    | 缓存（Cache）      | 展示用户程序Kernel函数内的L2 Cache访问情况，以便用户优化Cache命中率。 | 支持msprof采集的算子Profiling的bin文件。 |

- 服务化调优

  MindStudio Insight工具以时间线（Timeline）的呈现方式，将请求端到端的执行情况平铺在时间轴上，直观体现请求在各个关键阶段的耗时情况以及当下请求的状态信息，可帮助用户快速识别服务化性能瓶颈，并根据问题现象，调整调优策略。具体内容请参见[服务化调优](./docs/zh/user_guide/service_optimization.md)。

  | 功能界面           | 介绍                                                         | 场景说明                                |
  | ------------------ | ------------------------------------------------------------ | --------------------------------------- |
  | 时间线（Timeline） | 以时间线视图方式为用户提供请求端到端的执行情况，直观地查看请求在各个关键阶段的耗时情况以及当下请求的状态信息。 | 支持推理服务化请求trace数据的json文件。 |
  | 折线图（Curve）    | 以折线图和数据详情表的形式展示推理服务化进程中端到端的性能情况。 | 支持profiler.db文件。                   |

- 内存调优

  MindStudio Insight工具以图形化形式呈现device侧内存详细分配情况，并结合Python调用栈及自定义打点标签标记各种内存申请使用详情，进行内存问题的详细定位及调优。具体内容请参见[内存调优](./docs/zh/user_guide/memory_tuning.md)。

  | 功能界面          | 介绍                                                         | 场景说明                                      |
  | ----------------- | ------------------------------------------------------------ | --------------------------------------------- |
  | 内存详情（Leaks） | 通过调用栈图、折线块图和内存拆解图，将内存情况直观地呈现出来，便于开发者分析定位内存问题，有效缩短定位时间。 | 支持msLeaks工具采集到的db格式的内存结果文件。 |

## 🛠️ 贡献指南

欢迎为 MindStudio Insight 做贡献！请阅读[贡献指南](./CONTRIBUTING.md)了解如何参与项目开发。

## ⚖️ 相关信息

- [FAQ](./docs/zh/user_guide/FAQ.md)
- 许可证 <br>
  MindStudio Insight工具的使用许可证，具体请参见[LICENSE](./License)。<br>MindStudio Insight工具docs目录下的文档适用CC-BY 4.0许可证，具体请参见[DOC LICENSE](./docs/LICENSE)。
- [安全 && 免责声明](./DISCLAIMER.md)

## 🤝 建议与交流

欢迎大家为社区做贡献。如果有任何疑问或建议，请提交 [Issues](https://gitcode.com/Ascend/msinsight/issues)，我们会尽快回复。感谢您的支持。

诚邀参与[满意度问卷调查](https://rdccucd.wjx.cn/vm/PKPfKqO.aspx)抽取惊喜好礼😎。

| 💬 技术交流群 | 📢 官方公众号 | 🤝 更多加入渠道 |
| :---: | :---: | :--- |
| <img src="https://raw.gitcode.com/Ascend/msinsight/raw/master/docs/zh/user_guide/figures/readme/officialGroupChat.jpg" width="120"><br><sub>*扫码直接加入技术交流群*</sub> | <img src="https://raw.gitcode.com/Ascend/msinsight/raw/master/docs/zh/user_guide/figures/readme/officialAccount.jpg" width="120"><br><sub>*扫码关注获取最新动态*</sub> |欢迎扫码关注技术交流群跟官方公众号。这里是 MindStudio 用户与开发者最快捷的交流阵地：<br> **快速提问：** 与社区小伙伴即时探讨技术问题<br>**掌握动态：** 第一时间获取版本发布与功能更新通知<br> **经验共享：** 与其他开发者交流最佳实践  <br>🛠️ **其他渠道**：<br>👉 昇腾助手：[![WeChat](https://img.shields.io/badge/WeChat-07C160?style=flat-square&logo=wechat&logoColor=white)](https://gitcode.com/Ascend/msit/blob/master/docs/zh/figures/readme/xiaozhushou.png)<br>👉 昇腾论坛：[![Website](https://img.shields.io/badge/Website-%231e37ff?style=flat-square&logo=RSS&logoColor=white)](https://www.hiascend.com/forum/) |

## 🙏 致谢

MindStudio Insight由华为公司的下列部门联合贡献：

- 计算产品线

感谢来自社区的每一个PR，欢迎贡献MindStudio Insight！

## 关于MindStudio团队

华为MindStudio全流程开发工具链团队致力于提供端到端的昇腾AI应用开发解决方案，使能开发者高效完成训练开发、推理开发和算子开发。
