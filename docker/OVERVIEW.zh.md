# MindStudio Insight

## 快速参考

- MindStudio Insight 由 [MindStudio Insight community](https://gitcode.com/Ascend/msinsight) 维护。

- 从哪里获取帮助

  - [AscendHub镜像仓库](https://www.hiascend.com/developer/ascendhub)
  - [MindStudio Insight 代码仓](https://gitcode.com/Ascend/msinsight)
  - [MindStudio Insight 昇腾社区](https://www.hiascend.com/document/detail/zh/mindstudio/2600/GUI_baseddevelopmenttool/MindStudioInsight/docs/zh/user_guide/overview.md)
  - [问题反馈](https://gitcode.com/Ascend/msinsight/issues)

---

## 镜像介绍

**MindStudio Insight** 是专为昇腾 AI 开发者打造的深度可视化调优分析工具。它通过可视化手段呈现真实的软硬件运行数据，帮助开发者在天级时间内精准定位并解决性能瓶颈。

该 Docker 镜像提供开箱即用的 MindStudio Insight Web 化访问方式，适用于快速部署、数据分析环境隔离、团队共享分析服务以及在不同操作系统环境中统一运行 MindStudio Insight。

## 支持的 Tags 及 Dockerfile 链接

### Tag 规范

Tag 遵循以下格式：

```text
{版本号}-{操作系统}
```

| 字段 | 说明 | 示例值 |
| ------ | ------ | ------ |
| 版本号 | MindStudio Insight 版本 | `26.1.0` |
| 操作系统 | 镜像基础操作系统 | `ubuntu22.04`、`openeuler24.03` |

默认镜像以多架构 manifest 形式发布，同一 Tag 支持 `x86_64` 和 `aarch64`，Docker 会根据运行环境自动拉取匹配架构的镜像。

| Tag | 操作系统 | Dockerfile |
| ------ | ------ | ------ |
| `26.1.0-ubuntu22.04` | Ubuntu 22.04 | [Dockerfile.ubuntu](https://gitcode.com/Ascend/msinsight/blob/master/docker/Dockerfile.ubuntu) |
| `26.1.0-openeuler24.03` | openEuler 24.03 LTS | [Dockerfile.openEuler](https://gitcode.com/Ascend/msinsight/blob/master/docker/Dockerfile.openEuler) |

### 目录结构

```text
docker/
├── Dockerfile.ubuntu           # Ubuntu 22.04 镜像 Dockerfile
├── Dockerfile.openEuler        # openEuler 24.03 LTS 镜像 Dockerfile
├── entrypoint.sh               # 容器启动脚本
├── nginx.conf                  # HTTPS + mTLS nginx 配置
├── nginx-http.conf             # HTTP nginx 配置
├── ws-map.conf                 # WebSocket Upgrade 请求检测配置
├── supervisord.conf            # supervisor 进程管理配置
├── requirements.txt            # Python 运行时依赖
├── OVERVIEW.md                 # 英文版说明文档
└── OVERVIEW.zh.md              # 中文版说明文档
```

### 构建参数说明

| 参数 | 说明 | 默认值 |
| ------ | ------ | ------ |
| `VERSION` | MindStudio Insight 软件包版本 | `26.1.0` |
| `TAG` | MindStudio Insight 发布 Tag | `26.1.0` |
| `TARGETARCH` | Docker 构建目标架构，由 BuildKit/buildx 注入 | `amd64` 或 `arm64` |

Dockerfile 会将 Docker 架构名称映射为发布包架构名称：

| Docker `TARGETARCH` | 发布包架构 |
| ------ | ------ |
| `amd64` | `x86_64` |
| `arm64` | `aarch64` |

正式 Dockerfile 会在镜像构建过程中自动下载 MindStudio Insight 发布包：

```text
https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_${TAG}/MindStudio-Insight_${VERSION}_linux_${INSIGHT_ARCH}.zip
```

同时会下载对应的 `.sha256` 文件并进行校验，校验失败时构建会终止。

## 快速开始

### 运行

#### 推荐：HTTPS + mTLS 模式运行

**强烈建议在生产环境下使用 HTTPS + mTLS 模式。**

HTTPS + mTLS 模式借助 nginx 本身的 mTLS 双向认证能力，在 TLS 加密传输基础上要求客户端提供证书完成身份认证，适合生产环境、共享分析服务和需要访问控制的场景。

证书需由用户自行准备，并放置到合适的宿主机目录下，再将该证书目录挂载到容器的 `/etc/nginx/certs`。证书目录及其中私钥文件的访问权限需由用户自行保证安全，避免证书或私钥泄露。

如需启用 HTTPS + mTLS，请将证书目录挂载到 `/etc/nginx/certs`，目录中需包含：

```text
server.crt
server.key
ca.crt
```

运行示例：

```bash
# 示例中将容器 443 端口映射到宿主机 9443 端口，可按需替换宿主机端口
docker run -d \
  -p <host_https_port>:443 \
  -v /path/to/profile_data:/opt/insight/data \
  -v /path/to/certs:/etc/nginx/certs:ro \
  --name msinsight \
  msinsight:26.1.0-ubuntu22.04
```

例如使用宿主机端口 `9443` 时，浏览器访问：

```text
https://<host_ip>:9443
```

HTTPS + mTLS 模式下，浏览器还会通过客户端证书完成双向认证。

#### HTTP 模式运行（仅适合开发、测试或临时访问）

```bash
# 示例中将容器 80 端口映射到宿主机 9880 端口，可按需替换宿主机端口
docker run -d \
  -p <host_http_port>:80 \
  -v /path/to/profile_data:/opt/insight/data \
  --name msinsight \
  msinsight:26.1.0-ubuntu22.04
```

例如使用宿主机端口 `9880` 时，浏览器访问：

```text
http://<host_ip>:9880
```

> **注意：** HTTP 模式不提供传输层加密，也不适合作为生产环境默认使用方式。

#### 访问说明

上述示例中的 `<host_ip>` 为容器所在宿主机 IP 地址，需确保对应宿主机端口可从客户端网络连通。

容器会将根路径 `/` 重定向到 `/?proxy=true`，前端会连接当前浏览器端口，nginx 将 WebSocket 流量代理到容器内后端服务。

### 本地构建

#### 构建 Ubuntu 镜像

```bash
cd docker

docker build \
  -f Dockerfile.ubuntu \
  --build-arg VERSION={version} \
  --build-arg TAG={tag} \
  -t {msinsight_tag} .
```

#### 构建 openEuler 镜像

```bash
cd docker

docker build \
  -f Dockerfile.openEuler \
  --build-arg VERSION={version} \
  --build-arg TAG={tag} \
  -t {msinsight_tag} .
```

### 二次开发

可基于正式镜像制作自定义镜像：

```dockerfile
FROM msinsight:26.1.0-ubuntu22.04

# 根据需要添加自定义构建步骤。
```

## 许可证

MindStudio Insight 遵循 MulanPSL2 许可证，详见 [LICENSE](../License) 文件。使用本镜像前，请确保已了解并遵守 MindStudio Insight 及镜像中包含的第三方软件对应的许可证协议。

与所有容器镜像一样，预装软件包（Python、系统库等）可能受其自身许可证约束。
