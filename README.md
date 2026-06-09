<h1 align="center">MindStudio Insight</h1>

<div align="center">
  <img src="./modules/framework/public/favicon.ico" width="160" alt="MindStudio Insight Logo">
  <p><b><span style="font-size:24px;">昇腾 AI 全流程可视化调优利器</span></b></p>

  [![快速入门](https://badgen.net/badge/快速入门/QuickStart/blue)](./docs/zh/quick_start/system_tuning_quick_start.md)
  [![AI问答(DeepWiki)](https://badgen.net/badge/AI问答/DeepWiki/blue)](https://deepwiki.com/mindstudio-docs/master)
  [![AI问答(ZRead)](https://badgen.net/badge/AI问答/ZRead/blue)](https://zread.ai/mindstudio-docs/master)
  [![精确搜索](https://badgen.net/badge/精确搜索/ReadTheDocs/blue)](https://msinsight.readthedocs.io/zh-cn/latest/)
  [![昇腾社区](https://badgen.net/badge/昇腾社区/Community/blue)](https://www.hiascend.com/cn/developer/software/mindstudio)
  [![报告问题](https://badgen.net/badge/报告问题/Issues/blue)](https://gitcode.com/Ascend/msinsight/issues)
  [![Version](https://badgen.net/badge/Version/26.0.0/blue)](https://gitcode.com/Ascend/msinsight/releases)
</div>

## ✨ 最新消息

<span style="font-size:14px;">

🔹 **[2026.04.29]**：MindStudio Insight 26.0.0 版本上线，支持 Host Bound 问题定位、RL 性能分析和 Snapshot 内存大数据分析。

</span>

## ℹ️ 简介

**MindStudio Insight** 是面向昇腾 AI 开发者的可视化性能调优分析工具。它通过图形化方式呈现真实软硬件运行数据，帮助开发者快速定位系统、算子、服务化和内存等性能瓶颈。

| 核心价值 | 说明 |
| --- | --- |
| **全场景覆盖** | 支持系统调优、算子调优、服务化调优和内存调优。 |
| **大规模分析** | 支持百卡、千卡级集群分析，适配 20GB+ 性能数据处理。 |
| **便捷导入** | 自动遍历 Profiling 数据，减少手工合并和数据预处理成本。 |

<div align="left">
  <h4>▶️ 核心能力快速演示</h4>
  <img src="./assets/demo-system.gif" alt="系统调优演示" width="800">
  <p><sup>图示：系统调优数据导入与性能分析过程演示</sup></p>
</div>

## ⚙️ 功能介绍

MindStudio Insight 围绕昇腾 AI 性能分析主路径提供多维可视化调优能力：

| 功能名称 | 功能描述 | 详细说明 |
| --- | --- | --- |
| **系统调优** | 分析 Timeline、通信、内存、算子耗时等系统性能瓶颈。 | 《[系统调优](./docs/zh/user_guide/system_tuning.md)》 |
| **算子调优** | 展示指令流水、源码映射、负载分析和 Cache 等算子性能信息。 | 《[算子调优](./docs/zh/user_guide/operator_tuning.md)》 |
| **服务化调优** | 通过请求端到端 Timeline 和性能曲线定位推理服务化瓶颈。 | 《[服务化调优](./docs/zh/user_guide/service_optimization.md)》 |
| **内存调优** | 展示 device 侧内存分配、调用栈和标签信息，辅助定位内存问题。 | 《[内存调优](./docs/zh/user_guide/memory_tuning.md)》 |

## 🚀 快速入门

快速体验 MindStudio Insight 核心调优能力，请参见：<br>
🔹 《[系统调优快速入门](./docs/zh/quick_start/system_tuning_quick_start.md)》：学习如何使用概览、通信、时间线页签分析模型系统性能。<br>
🔹 《[算子调优快速入门](./docs/zh/quick_start/operator_tuning_quick_start.md)》：学习如何使用详情、时间线、源码页签分析算子性能。

## 📦 安装指南

介绍 MindStudio Insight 的环境依赖、软件包获取和安装方法，请参见《[MindStudio Insight 安装指南](./docs/zh/install_guide/mindstudio_insight_install_guide.md)》。

## 📘 使用指南

工具的详细使用方法和功能说明，请参见以下文档：<br>
🔹 《[产品概览](./docs/zh/user_guide/overview.md)》<br>
🔹 《[基础操作](./docs/zh/user_guide/basic_operations.md)》<br>
🔹 《[系统调优](./docs/zh/user_guide/system_tuning.md)》<br>
🔹 《[算子调优](./docs/zh/user_guide/operator_tuning.md)》<br>
🔹 《[服务化调优](./docs/zh/user_guide/service_optimization.md)》<br>
🔹 《[内存调优](./docs/zh/user_guide/memory_tuning.md)》

## 💡 典型案例

通过典型问题场景帮助用户理解并掌握工具使用：<br>
🔹 《[Host Bound 问题分析](./docs/zh/best_practices/Host_Bound_Analysis_with_Linux_Kernel_Trace.md)》<br>
🔹 《[Jupyter 插件安装指南](./docs/zh/best_practices/Jupyter_Plugin_Installation_Guide.md)》<br>
🔹 《[快捷键使用案例](./docs/zh/best_practices/Keyboard_Shortcuts.md)》<br>
🔹 《[Timeline 常见泳道与接口](./docs/zh/best_practices/Timeline_Common_Lanes_and_Interface.md)》<br>
🔹 《[verl Memory Snapshot 采集与分析](./docs/zh/best_practices/verl_Memory_Snapshot_Collection_and_Analysis.md)》

## ❓ FAQ

常见问题及解决方案，请参见《[MindStudio Insight FAQ](./docs/zh/support/faq.md)》。

## 🌌 智能检索

为提升文档查阅效率，我们提供多种高效检索方式：<br>
🔹 [AI 问答（DeepWiki）](https://deepwiki.com/mindstudio-docs/master)：自然语言问答，快速把握项目架构与模块关系。<br>
🔹 [AI 问答（ZRead）](https://zread.ai/mindstudio-docs/master)：中文问答体验更优，精准定位功能用法与细节。<br>
🔹 [精确搜索（ReadTheDocs）](https://msinsight.readthedocs.io/zh-cn/latest/)：关键词全文检索，直达安装、使用、FAQ 等文档。

## 🛠️ 贡献指南

欢迎参与 MindStudio Insight 项目建设：<br>
🔹 贡献流程请参见《[贡献指南](./CONTRIBUTING.md)》。<br>
🔹 开发环境、构建方式和项目结构说明请参见《[开发指南](./docs/zh/development_guide/develop_guide.md)》。

## ⚖️ 相关说明

🔹 《[版本说明](./docs/zh/release_notes/release_notes.md)》<br>
🔹 《[项目许可证](./License)》<br>
🔹 《[文档许可证](./docs/LICENSE)》<br>
🔹 《[安全声明](./docs/zh/legal/security_statement.md)》<br>
🔹 《[免责声明](./DISCLAIMER.md)》

## 🤝 建议与交流

欢迎大家为社区做贡献。如果有任何疑问或建议，请提交 [Issues](https://gitcode.com/Ascend/msinsight/issues)，我们会尽快回复。感谢您的支持。
诚邀参与[满意度问卷调查](https://rdccucd.wjx.cn/vm/PKPfKqO.aspx)抽取惊喜好礼😎。

|                                                  即时互动（微信群）                                                  |                                                 官方资讯（公众号）                                                 | 深度支持（助手/论坛）                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
|:-----------------------------------------------------------------------------------------------------------:|:---------------------------------------------------------------------------------------------------------:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| <img src="./docs/zh/user_guide/figures/readme/officialGroupChat.png" width="120"><br><sub>*扫码加入技术交流群*</sub> | <img src="./docs/zh/user_guide/figures/readme/officialAccount.png" width="120"><br><sub>*扫码关注官方公众号*</sub> | 扫码入群并关注公众号，直达 MindStudio 用户与开发者交流平台：<br>**快速提问：** 与社区小伙伴即时探讨技术问题<br>**掌握动态：** 第一时间获取版本发布与功能更新通知<br>**经验共享：** 与广大开发者交流最佳实践与实战心得<br><br>**更多支持渠道**：👉 昇腾助手 [![WeChat](https://img.shields.io/badge/WeChat-07C160?style=flat-square&logo=wechat&logoColor=white)](https://gitcode.com/Ascend/msit/blob/master/docs/zh/figures/readme/xiaozhushou.png) 👉 昇腾论坛：[![Website](https://img.shields.io/badge/Website-%231e37ff?style=flat-square&logo=RSS&logoColor=white)](https://www.hiascend.com/forum/) |

## 🙏 致谢

MindStudio Insight 由华为公司的下列部门联合贡献：<br>
🔹 计算产品线 <br>
🔹 2012 实验室

感谢来自社区的每一个 PR，欢迎贡献 MindStudio Insight！

华为 MindStudio 全流程开发工具链团队致力于提供端到端的昇腾 AI 应用开发解决方案，使能开发者高效完成训练开发、推理开发和算子开发。
