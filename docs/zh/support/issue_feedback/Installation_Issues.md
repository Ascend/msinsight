# 安装相关问题汇总

## MindStudio Insight 工具无法安装

### 问题描述

在社区下载的8.2和8.1的 MindStudio Insight 工具，都无法正常安装。

无论是普通用户，还是管理员用户双击都无法正常启动安装程序

### 解决方法

1. 检查兼容性设置
右键点击 .exe 文件，选择“属性”。
切换到“兼容性”标签页。
在“权限等级”区域，勾选“以管理员身份运行此程序”。
点击“应用”并确定。
2. 检查系统策略(高级操作)
通过快捷键 Win + R，输入 gpedit.msc 并回车打开本地组策略编辑器。
依次展开“计算机配置” -> “管理模板” -> “Windows组件” -> “Windows Installer”。
找到“禁止用户安装”的策略，双击打开。
确保该策略未被启用(如果已启用，请选择“未配置”或“禁用”)。
3. 重新下载文件
如果以上方法均无效，可能是文件下载过程中损坏了。
尝试重新从官方网站下载安装包，并重复以上步骤。
4. 检查防病毒软件
某些防病毒软件可能会阻止安装程序运行。
暂时禁用防病毒软件，然后再次尝试安装。
安装完成后，记得重新启用防病毒软件。

---

## 离线安装MindStudio Insight后无法使用

### 问题描述

报错 missing Dependencies，please install from https://developer.microsoft.com/en-US/microsoft-edge/webview2/#download-section

### 解决方法

【问题原因】

客户使用的 Windows 系统中没有安装 edge 浏览器。没有 webview2 工具存在

【解决方案】

下载并安装 edge 浏览器，或者直接从上面的链接下载
