# 安装说明与准备软件包

本文档介绍MindStudio Insight的安装方式与软件包准备，涵盖Windows、Linux、macOS系统的安装、JupyterLab插件与Docker镜像的使用，以及软件包下载与软件完整性校验，适用于需要安装和使用MindStudio Insight的开发者。

## 安装说明

MindStudio Insight是面向开发者的可视化调优工具，能够将性能数据以时序图、热力图等一些简单易懂的图表呈现，帮助开发者快速识别性能瓶颈，快速完成性能优化。本文主要介绍MindStudio Insight的安装方式。

MindStudio Insight可在如下场景使用，具体软件包下载可参见“[准备软件包](#准备软件包)”章节指引。

- 支持在Windows系统上安装使用，具体可参见《[安装指南-Windows系统](./mindstudio_insight_install_windows.md)》；<br>
- 支持在Linux系统上安装使用，具体可参见《[安装指南-Linux系统](./mindstudio_insight_install_linux.md)》；<br>
- 支持在macOS系统上安装使用，具体可参见《[安装指南-macOS系统](./mindstudio_insight_install_macos.md)》；<br>
- 支持通过安装JupyterLab插件方式使用，具体可参见《[安装指南-JupyterLab插件](./mindstudio_insight_install_jupyterlab.md)》；<br>
- 支持通过部署Docker镜像直接访问使用，具体可参见《[安装指南-Docker镜像](./mindstudio_insight_install_docker.md)》；<br>
- 支持插件开发功能，为开发者提供自主开发能力，开发者可自主开发插件包，并安装插件包，实现自主开发功能的使用，具体可参见《[安装指南-插件开发](./mindstudio_insight_install_plugin_dev.md)》。

## 准备软件包

### 软件包下载

您可以根据操作系统选择对应的软件包进行下载。下载二进制软件即表示您同意《[软件许可协议2.0](https://www.hiascend.com/legal/softlicense)》的条款和条件。

单击[MindStudio Insight Release](https://gitcode.com/Ascend/msinsight/releases)，确认版本信息后获取如[**表 1** 软件包清单](#软件包清单)所示软件包。

> [!NOTE]
>
> 表中软件包名称中的 `{version}` 为版本号占位符，实际下载时请替换为当前发布版本号，版本号获取请参见[版本发布说明](https://gitcode.com/Ascend/msinsight/releases)。

**表 1** 软件包清单<a id="软件包清单"></a>

|软件包|说明|
|--|--|
|MindStudio-Insight_*{version}*_win.exe|适用于Windows系统的MindStudio Insight软件包，含有GUI的集成开发环境。|
|MindStudio-Insight_*{version}*_linux-*{arch}*.zip|适用于Linux系统的MindStudio Insight软件包。|
|MindStudio-Insight_*{version}*_macos-*{arch}*.dmg|适用于macOS系统的MindStudio Insight软件包，含有GUI的集成开发环境。|
|mindstudio_insight_jupyterlab-*{version}*-py3-none-*{platform}*.whl|基于JupyterLab安装的软件包。|
|MindStudio-Insight_docker_image_*{version}*-*{os}*_*{Python_version}*_*{arch}*.tar|MindStudio Insight的Docker镜像包。|

### 软件完整性验证

为了防止软件包在传递过程或存储期间被恶意篡改，下载软件包时需下载对应软件包的.sha256文件用于完整性验证。

单击[MindStudio Insight Release](https://gitcode.com/Ascend/msinsight/releases)获取对应软件包的哈希校验文件（.sha256），并对[下载的软件包](#软件包下载)进行完整性校验。如果校验失败，请不要使用该软件包；如需支持与服务，请在论坛求助或提交技术工单。

具体校验方法如下：

1. 在本地获取软件包的SHA256校验码

    Windows系统，使用如下命令获取对应软件包的SHA256校验码。

    ```powershell
    certutil -hashfile 软件包名 SHA256
    ```

    macOS系统，使用如下命令获取对应软件包的SHA256校验码。

    ```shell
    shasum -a 256 软件包名
    ```

    Linux系统，使用如下命令获取对应软件包的SHA256校验码。

    ```bash
    sha256sum 软件包名
    ```

2. 打开查看对应哈希校验文件中的校验码，将该校验码和获取到的校验码（不区分大小写）进行比较，如果一致，则说明软件包通过完整性校验。
