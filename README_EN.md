<!-- md-trans-meta sourceCommit=8ba87048c67a1e13d2b4097e6b7e7ed22a41559a translatedAt=2026-08-17T09:15:14.033Z pushedAt=2026-08-17T09:15:32.958Z -->

<h1 align="center">MindStudio Insight</h1>
<div align="center">
  <img src="./modules/framework/public/favicon.ico" width="160" alt="MindStudio Insight Logo">
  <p><b><span style="font-size:24px;">A Powerful Visual Tuning Tool for the Full Ascend AI Workflow</span></b></p>

  [![Quick Start](https://badgen.net/badge/Quick%20Start/QuickStart/blue)](./docs/en/quick_start/system_tuning_quick_start.md)
  [![AI FAQ (DeepWiki)](https://badgen.net/badge/AI%20FAQ/DeepWiki/blue)](https://deepwiki.com/mindstudio-docs/master)
  [![AI FAQ (ZRead)](https://badgen.net/badge/AI%20FAQ/ZRead/blue)](https://zread.ai/mindstudio-docs/master)
  [![Exact Search](https://badgen.net/badge/Exact%20Search/ReadTheDocs/blue)](https://msinsight.readthedocs.io/zh-cn/latest/)
  [![Ascend Community](https://badgen.net/badge/Ascend%20Community/Community/blue)](https://www.hiascend.com/cn/developer/software/mindstudio)
  [![Report Issues](https://badgen.net/badge/Report%20Issues/Issues/blue)](https://gitcode.com/Ascend/msinsight/issues)
  [![Version](https://badgen.net/badge/Version/26.1.0/blue)](https://gitcode.com/Ascend/msinsight/releases)
</div>

English | [简体中文](./README.md)

## ✨ Latest News

<span style="font-size:14px;">

🔹 **[2026.07.25]**: MindStudio Insight 26.1.0 is released, with continuous enhancements in host-side performance analysis, memory snapshot analysis, containerized deployment, web access, and Timeline interaction experience, primarily targeting developers in Ascend AI performance tuning scenarios.

🔹 **[2026.04.29]**: MindStudio Insight 26.0.0 is released, supporting Host Bound issue locating, RL performance analysis, and Snapshot memory big data analysis.

</span>

## ℹ️ Introduction

**MindStudio Insight** is a visualized performance tuning and analysis tool for Ascend AI developers. It presents real software and hardware runtime data graphically, helping developers quickly locate performance bottlenecks in system, operator, serving, and memory tuning.

| Core Value | Description |
| --- | --- |
| **Full-Scenario Coverage** | Supports System Tuning, Operator Tuning, serving Tuning, and Memory Tuning. |
| **Large-Scale Analysis** | Supports analysis of hundred-rank and thousand-rank clusters, and adapts to processing 20 GB+ profile data. |
| **Convenient Import** | Automatically traverses profile data, reducing the cost of manual merging and data preprocessing. |

<div align="left">
  <h4>▶️ Core Capability Quick Demo</h4>
  <img src="./assets/demo-system.gif" alt="System Tuning Demo" width="800">
  <p><sup>Illustration: system tuning data import and performance analysis process</sup></p>
</div>

## ⚙️ Features

MindStudio Insight provides multi-dimensional visualized tuning capabilities around the Ascend AI performance analysis:

| Feature Name | Feature Description | Detailed Description |
| --- | --- | --- |
| **System Tuning** | Analyzes system performance bottlenecks such as Timeline, communication, memory, and operator duration. | *[System Tuning](./docs/en/user_guide/system_tuning.md)* |
| **Operator Tuning** | Displays operator performance information such as instruction pipeline, source code mapping, load analysis, and Cache. | *[Operator Tuning](./docs/en/user_guide/operator_tuning.md)* |
| **Serving Tuning** | Locates inference service bottlenecks through request end-to-end Timeline and performance curves. | *[Service-Oriented Tuning](./docs/en/user_guide/service_optimization.md)* |
| **Memory Tuning** | Displays device-side memory allocation, call stacks, and tag information to help locate memory issues. | *[Memory Tuning](./docs/en/user_guide/memory_tuning.md)* |

## 🚀 Quick Start

To quickly experience the core tuning capabilities of MindStudio Insight, see:<br>
🔹 *[System Tuning Quick Start](./docs/en/quick_start/system_tuning_quick_start.md)*: Learn how to use the **Summary**, **Communication**, and **Timeline** tabs to analyze model system performance.<br>
🔹 *[Operator Tuning Quick Start](./docs/en/quick_start/operator_tuning_quick_start.md)*: Learn how to use the **Details**, **Timeline**, and **Source** tabs to analyze operator performance.

## 📦 Installation Guide

For the environment dependencies, software package acquisition, and installation methods of MindStudio Insight, see *[MindStudio Insight Installation Guide](./docs/en/install_guide/mindstudio_insight_install_guide.md)*.

## 📘 User Guide

For detailed usage and feature descriptions of the tool, see the following documents:<br>
🔹 [Product Overview](./docs/en/user_guide/overview.md)<br>
🔹 [Basic Operations](./docs/en/user_guide/basic_operations.md)<br>
🔹 [System Tuning](./docs/en/user_guide/system_tuning.md)<br>
🔹 [Operator Tuning](./docs/en/user_guide/operator_tuning.md)<br>
🔹 [Serving Tuning](./docs/en/user_guide/service_optimization.md)<br>
🔹 [Memory Tuning](./docs/en/user_guide/memory_tuning.md)

## 💡 Typical Cases

Help users understand and master the tool through typical problem scenarios:<br>
🔹 [Host Bound Problem Analysis](./docs/en/best_practices/host_bound_analysis_with_linux_kernel_trace.md)<br>
🔹 [Jupyter Plugin Installation Guide](./docs/en/best_practices/Jupyter_Plugin_Installation_Guide.md)<br>
🔹 [Keyboard Shortcut Use Cases](./docs/en/best_practices/Keyboard_Shortcuts.md)<br>
🔹 [Timeline Common Lanes and Interfaces](./docs/en/best_practices/Timeline_Common_Lanes_and_Interface.md)<br>
🔹 [verl Memory Snapshot Collection and Analysis](./docs/en/best_practices/verl_Memory_Snapshot_Collection_and_Analysis.md)

## ❓ FAQs

For common issues and solutions, see [MindStudio Insight FAQs](./docs/en/support/faq.md).

## 🌌 Intelligent Search

To improve document lookup efficiency, we provide multiple efficient search methods:<br>
🔹 [AI FAQs (DeepWiki)](https://deepwiki.com/mindstudio-docs/master): Natural language Q&A to quickly grasp the project architecture and module relationships.<br>
🔹 [AI FAQs (ZRead)](https://zread.ai/mindstudio-docs/master): Better Chinese Q&A experience, precisely locating feature usage and details.<br>

## 🛠️ Contribution

Welcome to contribute to the MindStudio Insight project:<br>
🔹 For the contribution process, see [Contributing Guide](./CONTRIBUTING.md).<br>
🔹 For the development environment, build method, and project structure, see [Development Guide](./docs/en/development_guide/develop_guide.md).

## ⚖️ Related Notes

🔹 [Release Notes](./docs/en/release_notes/release_notes.md)<br>
🔹 [Project License](./License)<br>
🔹 [Documentation License](./docs/LICENSE)<br>
🔹 [Security Statement](./docs/en/legal/security_statement.md)<br>
🔹 [Disclaimer](./DISCLAIMER.md)

## 🤝 Suggestions and Communication

Everyone is welcome to contribute to the community. If you have any questions or suggestions, please submit them via [Issues](https://gitcode.com/Ascend/msinsight/issues), and we will respond as soon as possible. Thank you for your support.
You are cordially invited to participate in the [satisfaction survey](https://rdccucd.wjx.cn/vm/PKPfKqO.aspx) for a chance to win a surprise gift 😎.

|             Real-time Interaction (WeChat Group)             |             Official Updates (Official Account)              | In-depth Support (Assistant/Forum)                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: | :----------------------------------------------------------- |
| <img src="./docs/en/user_guide/figures/readme/officialGroupChat.png" width="120"><br><sub>*Scan the QR code to join the technical exchange group*</sub> | <img src="./docs/en/user_guide/figures/readme/officialAccount.png" width="120"><br><sub>*Scan the QR code to follow the official account*</sub> | Scan the QR code to join the group and follow the official account to access the MindStudio user and developer communication platform:<br>**Quick Questions:** Discuss technical issues with community members in real time<br>**Stay Updated:** Get version release and feature update notifications as soon as possible<br>**Experience Sharing:** Exchange best practices and hands-on insights with developers<br><br>**More Support Channels**: 👉 Ascend Assistant [![WeChat](https://img.shields.io/badge/WeChat-07C160?style=flat-square&logo=wechat&logoColor=white)](https://gitcode.com/Ascend/msit/blob/master/docs/zh/figures/readme/xiaozhushou.png) 👉 Ascend Forum: [![Website](https://img.shields.io/badge/Website-%231e37ff?style=flat-square&logo=RSS&logoColor=white)](https://www.hiascend.com/forum/) |

## 🙏 Acknowledgments

MindStudio Insight is jointly contributed by the following departments of Huawei:<br>
🔹 Computing Product Line <br>
🔹 2012 Laboratories

We appreciate every PR from the community and welcome contributions to MindStudio Insight!

The Huawei MindStudio full-process development toolchain team is committed to providing end-to-end Ascend AI app development solutions, enabling developers to efficiently complete training development, inference development, and operator development.
