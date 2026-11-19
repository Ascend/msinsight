# 安装指南-插件开发

## 安装MindStudio Insight

<a id="安装操作插件开发"></a>

### 安装操作（插件开发）

MindStudio Insight工具支持插件开发功能，为开发者提供自主开发能力，开发者可自主开发插件包，并安装插件包，实现自主开发功能的使用。

**开发插件**

开发者可自主开发插件，具体操作可参见[插件开发指南](https://gitcode.com/ascend/mstt/blob/poc/plugins/mindstudio-insight-plugins/document/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.md#%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97)。

插件包要求如下：

1. 插件包格式必须为ZIP压缩包。
2. 插件包中必须包含以下文件：

    - config.json配置文件。
    - 前端产物：必须为ZIP压缩包，包含前端asset目录及其文件和index.html文件。
    - 后端产物：必须为ZIP压缩包，包含对应平台及架构下的插件所需动态库和单个动态库文件。后端产物在config.json配置文件中的键值名为“backend\_\{_platform_\}\_\{_machine_\}”，其中platform为平台名称，machine为架构名称。例如，Linux x86环境下后端产物键值名为backend\_linux\_x86\_64。

    config.json配置文件格式要求如下：

    ```json
    {
        "pluginName":"插件名称",
        "frontend":"前端产物名称",
        "backend_{platform}_{machine}":"后端产物名称"
    }
    ```

    frontend为ZIP压缩包；backend_{platform}_{machine}为ZIP或动态库，platform为平台名称，machine为架构名称。

3. 插件包中包含的文件个数不能超过1000个，单个文件大小不能超过200MB。
4. 插件包需具有当前用户属主，具有可读可写权限，不支持链接文件和包含链接的文件。

> [!NOTE]
>
> MindStudio Insight工具支持通过“.so”形式加载任何插件，请务必对所需插件包进行完整性校验，保证其来源安全可信，从而有效避免社区投毒、恶意代码注入等潜在安全风险。

**安装插件**

进入MindStudio Insight工具的安装目录，执行以下命令，安装已开发的插件包。其中**plugin package path**为插件包所在路径。

```shell
python resources/profiler/plugin_install.py install --path="plugin package path"
```

**使用插件**

安装完成后，打开MindStudio Insight工具，导入数据即可正常使用。

如果插件包使用的是自主开发的唤醒逻辑，则依据实际情况进行使用。
