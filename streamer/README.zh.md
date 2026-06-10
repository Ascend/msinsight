# MindStudio Insight Streamer 脚本说明

MindStudio Insight Streamer 脚本用于在本地 Docker 环境中快速运行 MindStudio Insight，并通过浏览器访问 Insight Web 页面。脚本会辅助完成镜像选择、端口映射、数据目录挂载、HTTPS + mTLS 证书挂载以及容器停止等操作。

## 能力概览

- 自动识别本地最新的 `msinsight` 镜像。
- 支持显式指定镜像。
- 支持 HTTP 模式运行。
- 支持 HTTPS + mTLS 模式运行。
- 支持挂载宿主机数据目录到容器内 `/opt/insight/data`。
- 支持将证书目录挂载到容器内 `/etc/nginx/certs`。
- 支持默认端口、指定端口和动态端口。
- 支持透传额外 `docker run` 参数。
- 支持按容器名、端口或镜像仓库停止容器。

## 文件说明

| 文件 | 说明 |
|---|---|
| `run_insight_streamer.py` | 启动本地 MindStudio Insight Docker 容器。 |
| `stop_insight_streamer.py` | 停止本地 MindStudio Insight Docker 容器。 |

## 前置条件

- 已安装并启动 Docker。
- 本地存在 MindStudio Insight 镜像。
- 如果希望脚本自动识别镜像，镜像仓库名应为 `msinsight`。

示例镜像名：

```text
msinsight:26.1.0-ubuntu22.04
```

如果本地没有 `msinsight` 镜像，请先下载或构建镜像。推荐下载地址：

```text
https://www.hiascend.com/developer/ascendhub
```

如果镜像名称不是 `msinsight`，可以通过 `--image` 显式指定。

## 启动 Insight

以下命令请在 `streamer` 目录下执行。

### HTTP 模式

```bash
python3 run_insight_streamer.py \
  -v /path/to/profile_data
```

该命令会使用本地最新的 `msinsight` 镜像启动容器，并完成如下映射：

```text
<宿主机数据目录> -> /opt/insight/data
<宿主机 HTTP 端口> -> 容器 80 端口
```

默认 HTTP 端口：

```text
8080
```

访问地址：

```text
http://<host_ip>:8080/?proxy=true
```

### HTTPS + mTLS 模式

```bash
python3 run_insight_streamer.py \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

证书目录中必须包含：

```text
server.crt
server.key
ca.crt
```

脚本会将证书目录只读挂载到容器固定路径：

```text
/etc/nginx/certs
```

该路径与镜像内 nginx 配置和 entrypoint 启动逻辑一致。挂载后，容器会自动进入 HTTPS + mTLS 模式。

默认 HTTPS 端口：

```text
8443
```

访问地址：

```text
https://<host_ip>:8443/?proxy=true
```

健康检查示例：

```bash
curl -k \
  --cert /path/to/client.crt \
  --key /path/to/client.key \
  https://<host_ip>:8443/health
```

## 参数说明

### `run_insight_streamer.py` 参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--image` | 自动选择本地最新 `msinsight` 镜像 | 指定要启动的镜像。若不指定，脚本会扫描本地仓库名为 `msinsight` 的镜像，并按 tag 选择最新版本。 |
| `-n`, `--container-name` | `msinsight-streamer` | 指定容器名称。如果同名容器已存在，可通过 `-y` 自动替换。 |
| `--host-ip` | 自动探测宿主机 IP | 仅用于启动完成后打印访问 URL，不影响 Docker 端口绑定。 |
| `--http-port` | `8080` | HTTP 模式下的宿主机端口。可设置为 `dynamic`，表示优先尝试默认端口，不可用时自动选择空闲端口。 |
| `--https-port` | `8443` | HTTPS + mTLS 模式下的宿主机端口。可设置为 `dynamic`。 |
| `-v`, `--volume` | 无 | 宿主机数据目录，推荐配置。脚本会挂载到容器内 `/opt/insight/data`。可重复指定。 |
| `--data-mount` | `/opt/insight/data` | 容器内数据目录挂载路径。通常不建议修改，除非确认 Insight 支持其他数据目录。 |
| `--cert-dir` | 无 | 宿主机证书目录。指定后默认进入 HTTPS + mTLS 模式，并固定挂载到容器内 `/etc/nginx/certs`。 |
| `--mode` | `auto` | 运行模式：`auto`、`http`、`mtls`。`auto` 表示传入 `--cert-dir` 时启用 mTLS，否则使用 HTTP。 |
| `-y`, `--yes` | `false` | 如果同名容器已存在，自动停止并删除旧容器，无需交互确认。 |
| `--` 后参数 | 无 | 透传给 `docker run` 的原生参数，例如 `-- --cpus 2 --memory 4g`。 |

### `stop_insight_streamer.py` 参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `-n`, `--container-name` | `msinsight-streamer` | 按容器名停止指定容器。 |
| `-p`, `--port` | 无 | 按宿主机暴露端口匹配并停止容器，例如 `-p 8443`。 |
| `-a`, `--all` | `false` | 停止所有运行中且镜像仓库名为 `msinsight` 的容器。 |
| `-y`, `--yes` | `false` | 自动确认停止操作，无需交互确认。 |

## 常用示例

### 指定镜像

```bash
python3 run_insight_streamer.py \
  --image msinsight:26.1.0-ubuntu22.04 \
  -v /path/to/profile_data
```

### 指定 HTTPS 端口

```bash
python3 run_insight_streamer.py \
  --image msinsight:26.1.0-ubuntu22.04 \
  --https-port 9880 \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

访问地址：

```text
https://<host_ip>:9880/?proxy=true
```

### 动态端口

HTTP 模式：

```bash
python3 run_insight_streamer.py \
  --http-port dynamic \
  -v /path/to/profile_data
```

HTTPS + mTLS 模式：

```bash
python3 run_insight_streamer.py \
  --https-port dynamic \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

脚本会优先尝试默认端口；如果默认端口不可用，则由操作系统分配一个空闲端口。

### 传递额外 Docker 参数

`--` 之后的参数会原样传递给 `docker run`：

```bash
python3 run_insight_streamer.py \
  -v /path/to/profile_data \
  -- --cpus 2 --memory 4g
```

## 数据目录挂载

`-v` 参数表示宿主机数据目录，不是完整的 Docker `host:container` 语法。

示例：

```bash
-v /home/user/profile_data
```

脚本会自动转换为：

```text
-v /home/user/profile_data:/opt/insight/data
```

如果指定多个 `-v`，第一个目录会挂载到 `/opt/insight/data`，后续目录会挂载到带后缀的路径，例如 `/opt/insight/data-1`。

## 停止 Insight

### 停止默认容器

```bash
python3 stop_insight_streamer.py
```

### 无需确认直接停止

```bash
python3 stop_insight_streamer.py -y
```

### 按容器名停止

```bash
python3 stop_insight_streamer.py -n msinsight-streamer
```

### 按宿主机端口停止

```bash
python3 stop_insight_streamer.py -p 8443
```

### 停止所有运行中的 msinsight 容器

```bash
python3 stop_insight_streamer.py -a
```

## 容器生命周期

启动脚本使用了：

```text
--rm
```

因此容器停止后，Docker 会自动删除容器对象。如果数据通过 `-v` 挂载在宿主机目录中，数据不会因容器停止而丢失。

如果停止后需要重新打开 Insight，只需再次执行 `run_insight_streamer.py`，并传入相同的镜像、数据目录、证书目录和端口参数即可。

## 注意事项

- 自动镜像识别只检查仓库名为 `msinsight` 的镜像。
- 如果镜像名不同，请使用 `--image` 显式指定。
- HTTP 模式仅适合本地验证或临时使用。
- HTTPS + mTLS 模式要求证书目录中包含 `server.crt`、`server.key`、`ca.crt`。
- 证书挂载路径固定为 `/etc/nginx/certs`，因为镜像 entrypoint 和 nginx 配置均使用该路径。
