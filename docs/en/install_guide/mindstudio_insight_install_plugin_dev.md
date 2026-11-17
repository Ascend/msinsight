# Installing MindStudio Insight for Plugin Development

## Installing MindStudio Insight

<a id="installation-operation-plugin-development"></a>

### Installation Operation (Plugin Development)

The MindStudio Insight tool supports plugin development, providing developers with the ability to develop independently. Developers can independently develop plugin packages and install them to use custom-developed features.

**Developing Plugins**

Developers can independently develop plugins. For details, see [Plugin Development Guide](https://gitcode.com/ascend/mstt/blob/poc/plugins/mindstudio-insight-plugins/document/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.md#%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97).

The plugin package requirements are as follows:

1. The plugin package must be in ZIP format.
2. The plugin package must contain the following files:

    - `config.json` configuration file.
    - Frontend artifact: must be a ZIP package, containing the frontend `asset` directory and its files, as well as the `index.html` file.
    - Backend artifact: must be a ZIP package, containing the dynamic libraries required by the plugin for the corresponding platform and architecture, and a single dynamic library file. The key name for the backend artifact in the `config.json` configuration file is `backend_{platform}_{machine}`, where `platform` is the platform name and `machine` is the architecture name. For example, in a Linux x86 environment, the backend artifact key name is `backend_linux_x86_64`.

    The format requirements for the `config.json` configuration file are as follows:

    ```json
    {
        "pluginName":"plugin name",
        "frontend":"frontend artifact name",                      # zip archive
        "backend_{platform}_{machine}":"backend artifact name"  # zip or dynamic library
    }
    ```

    Where `platform` is the platform name and `machine` is the architecture name.

3. The number of files contained in the plugin package cannot exceed 1000, and the size of a single file cannot exceed 200 MB.
4. The plugin package must be owned by the current user, have read and write permissions, and must not support linked files or files containing links.

> [!NOTE]
>
> The MindStudio Insight tool supports loading any plugin in the `.so` format. You must perform an integrity check on the required plugin package to ensure that its source is secure and trustworthy, thereby effectively avoiding potential security risks such as community poisoning and malicious code injection.

**Installing the Plugin**

Go to the installation directory of the MindStudio Insight tool and run the following command to install the developed plugin package. `plugin package path` is the path where the plugin package is located.

```shell
python resources/profiler/plugin_install.py install --path="plugin package path"
```

**Using the Plugin**

After the installation is complete, open the MindStudio Insight tool and import data to use it normally.

If the plugin package uses a self-developed wake-up logic, use it according to the actual situation.
