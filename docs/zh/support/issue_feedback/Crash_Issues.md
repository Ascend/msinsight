# 奔溃类问题常见处理措施

## MindStudio Insight 打开后立即闪退

### 问题描述

MindStudio Insight 打开后立即闪退，请问怎么处理，安装edge webview2 runtime报这个错

![image](figures/Crash_Issues/webview2-install-error.png)

【环境信息】

工具版本：MindStudio Insight 8.2.RC1

操作系统：win11

### 解决方法

【问题现象】

安装Insight后，启动闪退，无法正常使用，尝试安装依赖Webview2 Runtime同样报错“已安装”。

【定位进展】

1、启动后闪退时未生成profiler_server_x_x.log，定界为启动时的操作系统相关依赖问题；

2、与发帖人沟通，曾清理过C盘下部分文件，可能与清理时误删WebView2 Runtime或破坏了依赖完整性所致；

【规避/修复方案】

1、难以直接定位缺失依赖，及针对性修复。建议尝试重装MSVC后重装WebView2 Runtime；

2、在环境难以修复的情况下，可临时通过浏览器访问方式使用（需自行确保环境安全）

---

## MindStudio Insight一直闪退，重装也无法解决

### 问题描述

MindStudio Insight 一直闪退，重装也无法解决

【解决方案】

黄区可以找 12345 客服获取代理下载 edge webview2 runtime 工具。

启动包在 Windows 官网下载。

### 解决方法

【问题分析】

发现 MindStudio Insight 打开就立刻闪退。

用户使用 Windows 11，安装目录中缺少 .mindstudio_insight 文件夹，检查 Edge 浏览器也不能打开。初步怀疑是 Edge 浏览器损坏导致 Insight 无法打开。

Insight 前端依赖 Edge 浏览器打开，如果 Edge 浏览器损坏，Insight 也不能正确启动。
