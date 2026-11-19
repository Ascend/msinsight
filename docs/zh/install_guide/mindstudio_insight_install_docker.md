# 安装指南：Docker镜像

## 安装MindStudio Insight

<a id="安装操作镜像"></a>

### 安装操作（镜像）

**准备环境**

1. 准备一台已安装Docker且Docker服务运行正常的Linux服务器。
2. 根据服务器的网络环境选择镜像获取方式：

    - 在线获取：确保服务器能够访问AscendHub镜像仓库。
    - 离线获取：根据服务器的CPU架构下载对应架构的MindStudio Insight镜像包，并完成[软件完整性验证](./mindstudio_insight_install_overview.md#软件完整性验证)。

3. 确保客户端能够访问服务器上用于MindStudio Insight的映射端口。

> [!NOTE]
>
> 镜像中的操作系统和Python运行环境已经预置，无需在服务器上另行安装MindStudio Insight的运行依赖。如果需要自行构建镜像，请参见[MindStudio Insight镜像说明](../../../docker/OVERVIEW.zh.md#本地构建)。

**获取镜像**

用户可根据服务器的网络环境，通过AscendHub镜像仓库在线拉取镜像，或加载已下载的离线镜像包。

<a id="在线拉取镜像"></a>

#### 方式一：在线拉取镜像

1. 访问[AscendHub MindStudio Insight镜像页面](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1)，根据镜像基础操作系统和Python版本选择镜像Tag。

    以下镜像Tag为最新版本，均支持`x86_64`和`aarch64`架构。

    |镜像Tag|基础操作系统|Python版本|
    |--|--|--|
    | [`26.2.0-ubuntu22.04-py3.10`](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1?version=26.2.0-ubuntu22.04-py3.10) |Ubuntu 22.04|3.10|
    | [`26.2.0-openeuler24.03-py3.11`](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1?version=26.2.0-openeuler24.03-py3.11) |openEuler 24.03 LTS|3.11|

2. 在镜像页面对应Tag的操作栏中单击“立即下载”，按照页面显示的操作步骤复制并执行镜像拉取命令。

    > [!NOTE]
    >
    > AscendHub镜像仓库中的镜像地址和拉取命令可能随仓库配置变化，请以镜像页面实时显示的信息为准。

3. 执行以下命令，确认镜像已拉取成功：

    ```shell
    docker images
    ```

    在回显中找到已拉取的MindStudio Insight镜像，并记录镜像名称和Tag。下文使用`{image_name}:{image_tag}`表示该镜像。

<a id="加载离线镜像"></a>

#### 方式二：加载离线镜像

1. 将下载的镜像包上传至服务器。
2. 在镜像包所在目录执行以下命令，加载镜像。

    ```shell
    docker load -i MindStudio-Insight_docker_image_{version}-{os}_{Python_version}_{arch}.tar
    ```

    命令回显中将显示已加载镜像的名称和Tag，例如：

    ```output
    Loaded image: {image_name}:{image_tag}
    ```

3. 执行以下命令，确认镜像已加载成功：

    ```shell
    docker images
    ```

    在回显中找到已加载的MindStudio Insight镜像，并记录镜像名称和Tag。下文使用`{image_name}:{image_tag}`表示该镜像。

**运行镜像**

根据使用场景，选择HTTPS + mTLS模式或HTTP模式运行MindStudio Insight镜像。生产环境或多人共享服务场景推荐使用HTTPS + mTLS模式。

推荐优先使用MindStudio Insight Streamer脚本控制容器启停。脚本可自动完成镜像选择、端口映射、数据和证书目录挂载，并在容器停止后自动删除容器对象。也可根据实际需要直接使用Docker命令运行镜像。

<a id="使用streamer脚本运行镜像"></a>

#### 方式一：使用Streamer脚本运行镜像（推荐）

1. 从[MindStudio Insight代码仓](https://gitcode.com/Ascend/msinsight)获取`streamer`目录，并在服务器上进入该目录。
2. 根据获取镜像后的镜像名称选择镜像：

    - 如果镜像仓库名为`msinsight`，脚本会自动选择本地最新的`msinsight`镜像，无需指定`--image`。
    - 如果镜像仓库名不是`msinsight`，执行脚本时需要通过`--image {image_name}:{image_tag}`指定镜像。

3. 根据使用场景启动MindStudio Insight。

    - HTTPS + mTLS模式（推荐）

        1. 准备服务器证书、服务器私钥和CA证书，并将证书文件放置在服务器的同一目录下。文件名必须如下：

            ```text
            server.crt
            server.key
            ca.crt
            ```

            > [!NOTE]
            >
            > 请妥善设置证书目录及私钥文件的访问权限，防止证书或私钥泄露。客户端还需配置由该CA签发的客户端证书，以完成mTLS双向认证。

        2. 执行以下命令，启动容器。

            ```shell
            python3 run_insight_streamer.py \
              --image {image_name}:{image_tag} \
              --cert-dir /path/to/certs \
              -v /path/to/profile_data
            ```

            如果镜像仓库名为`msinsight`，可省略`--image`参数。脚本默认将服务器的`8443`端口映射至容器的HTTPS端口。如果默认端口已被占用，可增加`--https-port dynamic`参数自动选择可用端口，或通过`--https-port {host_https_port}`指定端口。

        3. 根据命令回显中的URL，在已配置客户端证书的浏览器中打开MindStudio Insight。默认访问地址如下：

            ```text
            https://{host_ip}:8443/?proxy=true
            ```

    - HTTP模式（仅适用于开发、测试或临时访问）

        1. 执行以下命令，启动容器。

            ```shell
            python3 run_insight_streamer.py \
              --image {image_name}:{image_tag} \
              -v /path/to/profile_data
            ```

            如果镜像仓库名为`msinsight`，可省略`--image`参数。脚本默认将服务器的`8080`端口映射至容器的HTTP端口。如果默认端口已被占用，可增加`--http-port dynamic`参数自动选择可用端口，或通过`--http-port {host_http_port}`指定端口。

        2. 根据命令回显中的URL，在浏览器中打开MindStudio Insight。默认访问地址如下：

            ```text
            http://{host_ip}:8080/?proxy=true
            ```

        > [!WARNING] 警告
        > HTTP模式不提供传输层加密和客户端身份认证，请勿在生产环境或不可信网络中使用。

4. 如需停止通过Streamer脚本启动的默认容器，请在`streamer`目录下执行以下命令：

    ```shell
    python3 stop_insight_streamer.py
    ```

    更多镜像指定、端口配置、容器停止及Docker参数透传方式，请参见[MindStudio Insight Streamer脚本说明](../../../streamer/README.zh.md)。

<a id="使用docker命令运行镜像"></a>

#### 方式二：使用Docker命令运行镜像

- HTTPS + mTLS模式（推荐）

    1. 按照[方式一](#使用streamer脚本运行镜像)中的证书要求准备证书。
    2. 执行以下命令，启动容器。

        ```shell
        docker run -d \
          -p {host_https_port}:443 \
          -v /path/to/profile_data:/opt/insight/data \
          -v /path/to/certs:/etc/nginx/certs:ro \
          --name msinsight \
          {image_name}:{image_tag}
        ```

    3. 在已配置客户端证书的浏览器中访问以下地址，打开MindStudio Insight。

        ```text
        https://{host_ip}:{host_https_port}/?proxy=true
        ```

- HTTP模式（仅适用于开发、测试或临时访问）

    1. 执行以下命令，启动容器。

        ```shell
        docker run -d \
          -p {host_http_port}:80 \
          -v /path/to/profile_data:/opt/insight/data \
          --name msinsight \
          {image_name}:{image_tag}
        ```

    2. 在浏览器中访问以下地址，打开MindStudio Insight。

        ```text
        http://{host_ip}:{host_http_port}/?proxy=true
        ```

    > [!WARNING] 警告
    > HTTP模式不提供传输层加密和客户端身份认证，请勿在生产环境或不可信网络中使用。

相关参数说明如下：

|参数|说明|
|--|--|
|{host_https_port}|服务器上用于访问MindStudio Insight的HTTPS端口，例如`9443`。|
|{host_http_port}|服务器上用于访问MindStudio Insight的HTTP端口，例如`9880`。|
|/path/to/profile_data|服务器上存放待分析性能数据的目录，请替换为实际的绝对路径。|
|/path/to/certs|服务器上存放mTLS证书的目录，请替换为实际的绝对路径。|
|msinsight|容器名称，可根据实际情况修改。|
|{image_name}:{image_tag}|在线拉取或加载离线镜像后得到的镜像名称和Tag。|

执行以下命令，可查看容器是否启动成功：

```shell
docker ps --filter name=msinsight
```

如果容器未正常运行，可执行以下命令查看容器日志：

```shell
docker logs msinsight
```

> [!NOTE]
>
> - `{host_ip}`为运行容器的服务器IP地址。请确保客户端与服务器网络连通，且服务器防火墙已放通相应的映射端口。
> - 挂载到`/opt/insight/data`的服务器目录可在MindStudio Insight中用于访问待分析性能数据。
> - 使用Docker命令启动容器时，仅当`/etc/nginx/certs`目录中同时存在`server.crt`、`server.key`和`ca.crt`时，才会启用HTTPS + mTLS模式；否则会启用HTTP模式。

## 升级MindStudio Insight

如果需要升级MindStudio Insight，需先卸载已安装的MindStudio Insight，再获取最新MindStudio Insight镜像包重新安装。

请根据实际场景，参见[卸载MindStudio Insight](#卸载mindstudio-insight)章节内容完成卸载操作，并重新安装最新MindStudio Insight镜像包。

## 卸载MindStudio Insight

<a id="卸载操作镜像"></a>

### 卸载操作（镜像）

1. 停止并删除MindStudio Insight容器。

    - 如果使用Streamer脚本启动容器，推荐在`streamer`目录下执行以下命令停止默认容器：

        ```shell
        python3 stop_insight_streamer.py
        ```

        Streamer启动的容器配置了`--rm`参数，容器停止后会自动删除容器对象。如果启动时指定了其他容器名称，可通过`-n`指定该名称：

        ```shell
        python3 stop_insight_streamer.py -n {container_name}
        ```

    - 如果使用Docker命令启动容器，执行以下命令停止并删除容器：

        ```shell
        docker rm -f {container_name}
        ```

        其中，`{container_name}`为启动容器时指定的容器名称，例如`msinsight`。

2. 执行以下命令，确认没有需要保留的容器正在使用待删除镜像：

    ```shell
    docker ps -a --filter ancestor={image_name}:{image_tag}
    ```

3. 执行以下命令，删除MindStudio Insight镜像：

    ```shell
    docker rmi {image_name}:{image_tag}
    ```

    其中，`{image_name}:{image_tag}`为加载镜像时记录的镜像名称和Tag，可通过`docker images`查询。

> [!NOTE]
>
> - 删除容器和镜像不会删除通过`-v`挂载的服务器数据目录。如果确认不再需要其中的性能数据，请根据实际挂载路径手动清理。
> - 如果`docker rmi`提示镜像正在使用，请先根据回显信息确认并删除关联容器，再重新执行镜像删除命令。不要在未确认容器用途的情况下强制删除镜像。
> - 更多按端口停止容器、停止所有运行中的`msinsight`容器等方式，请参见[MindStudio Insight Streamer脚本说明](../../../streamer/README.zh.md#停止-insight)。
