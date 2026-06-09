# MindStudio Insight Docker Image Overview

## Quick Reference

| Item | Description |
| ------ | ------ |
| **Image Name** | msinsight |
| **Maintainer** | MindStudio Insight Team |
| **Source Repository** | [https://gitcode.com/Ascend/msinsight](https://gitcode.com/Ascend/msinsight) |
| **Dockerfile Path** | `docker/Dockerfile.ubuntu`, `docker/Dockerfile.openEuler` |
| **License** | MulanPSL2 |
| **Default Version** | `26.1.0` |
| **Supported Architectures** | `x86_64`, `aarch64` |
| **Default Service Ports** | `80`, `443` |

## Image Introduction

**MindStudio Insight** is a deep visual tuning and analysis tool designed for Atlas AI developers. It presents real software and hardware runtime data through visualization, helping developers accurately locate and resolve performance bottlenecks within days.

This Docker image provides an out-of-the-box web access mode for MindStudio Insight. It is suitable for quick deployment, isolated data analysis environments, shared team analysis services, and consistent MindStudio Insight usage across different operating systems.

## Image Tag Description and Dockerfile Archive Path

### Image Tag Description

Recommended image tag format:

```text
{Version}-{OS}
```

| Field | Description | Example Value |
| ------ | ------ | ------ |
| Version | MindStudio Insight version | `26.1.0` |
| OS | Base operating system of the image | `ubuntu22.04`, `openeuler24.03` |

By default, images are published as multi-architecture manifests. The same tag supports both `x86_64` and `aarch64`, and Docker automatically pulls the image matching the runtime platform.

Examples:

| Tag | Operating System | Supported Architectures |
| ------ | ------ | ------ |
| `26.1.0-ubuntu22.04` | Ubuntu 22.04 | x86_64, aarch64 |
| `26.1.0-openeuler24.03` | openEuler 24.03 LTS | x86_64, aarch64 |

### Dockerfile Archive Path

```text
docker/Dockerfile.ubuntu
docker/Dockerfile.openEuler
```

### Directory Structure

```text
docker/
├── Dockerfile.ubuntu           # Dockerfile for Ubuntu 22.04 image
├── Dockerfile.openEuler        # Dockerfile for openEuler 24.03 LTS image
├── entrypoint.sh               # Container startup script
├── nginx.conf                  # HTTPS + mTLS nginx configuration
├── nginx-http.conf             # HTTP nginx configuration
├── ws-map.conf                 # WebSocket upgrade detection map
├── supervisord.conf            # supervisor process management configuration
├── requirements.txt            # Python runtime dependencies
├── OVERVIEW.md                 # English overview document
└── OVERVIEW.zh.md              # Chinese overview document
```

### Build Argument Description

| Parameter | Description | Default |
| ------ | ------ | ------ |
| `VERSION` | MindStudio Insight package version | `26.1.0` |
| `TAG` | MindStudio Insight release tag | `26.1.0` |
| `TARGETARCH` | Docker build target architecture injected by BuildKit/buildx | `amd64` or `arm64` |

The Dockerfiles map Docker architecture names to release package architecture names:

| Docker `TARGETARCH` | Release Package Architecture |
| ------ | ------ |
| `amd64` | `x86_64` |
| `arm64` | `aarch64` |

The official Dockerfiles automatically download the MindStudio Insight release package during image build:

```text
https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_${TAG}/MindStudio-Insight_${VERSION}_linux_${INSIGHT_ARCH}.zip
```

The corresponding `.sha256` file is also downloaded and verified. If checksum verification fails, the build exits immediately.

## Quick Start

### Run

#### Recommended: Run in HTTPS + mTLS Mode

**HTTPS + mTLS mode is strongly recommended for production environments.**

HTTPS + mTLS mode uses nginx's built-in mTLS mutual authentication capability. In addition to TLS encrypted transport, the client must provide a certificate for identity authentication. This mode is suitable for production environments, shared analysis services, and scenarios that require access control.

Users must prepare the certificates, place them in an appropriate directory on the host, and mount the certificate directory to `/etc/nginx/certs` in the container. Users are responsible for securing the certificate directory and private key files to prevent certificate or private key leakage.

To enable HTTPS + mTLS, mount the certificate directory to `/etc/nginx/certs`. The directory must contain:

```text
server.crt
server.key
ca.crt
```

Example:

```bash
# This example maps container port 443 to host port 9443. Replace the host port as needed.
docker run -d \
  -p <host_https_port>:443 \
  -v /path/to/profile_data:/opt/insight/data \
  -v /path/to/certs:/etc/nginx/certs:ro \
  --name msinsight \
  msinsight:26.1.0-ubuntu22.04
```

For example, when using host port `9443`, open the following URL in a browser:

```text
https://<host_ip>:9443
```

In HTTPS + mTLS mode, the browser also completes mutual authentication by using a client certificate.

#### Run in HTTP Mode (for development, testing, or temporary access only)

```bash
# This example maps container port 80 to host port 9880. Replace the host port as needed.
docker run -d \
  -p <host_http_port>:80 \
  -v /path/to/profile_data:/opt/insight/data \
  --name msinsight \
  msinsight:26.1.0-ubuntu22.04
```

For example, when using host port `9880`, open the following URL in a browser:

```text
http://<host_ip>:9880
```

> **Note:** HTTP mode does not provide transport-layer encryption and is not suitable as the default mode for production environments.

#### Access Notes

In the examples above, `<host_ip>` is the IP address of the host where the container runs. Ensure that the corresponding host port is reachable from the client network.

The container redirects `/` to `/?proxy=true`. The frontend connects to the current browser port, and nginx proxies WebSocket traffic to the backend service inside the container.

### Local Build

#### Build Ubuntu Image

```bash
cd docker

docker build \
  -f Dockerfile.ubuntu \
  --build-arg VERSION={version} \
  --build-arg TAG={tag} \
  -t {msinsight_tag} .
```

#### Build openEuler Image

```bash
cd docker

docker build \
  -f Dockerfile.openEuler \
  --build-arg VERSION={version} \
  --build-arg TAG={tag} \
  -t msinsight:26.1.0-openeuler24.03-x86_64 .
```

### Secondary Development

You can create a custom image based on the official image:

```dockerfile
FROM msinsight:26.1.0-ubuntu22.04

# Add custom build steps as needed.
```

Build and run:

```bash
docker build -t my-msinsight:latest .

docker run -d \
  -p 9880:80 \
  -v /path/to/profile_data:/opt/insight/data \
  --name my-msinsight \
  my-msinsight:latest
```

## License

MindStudio Insight is licensed under MulanPSL2. For details, see the [LICENSE](../License) file. Before using this image, ensure that you understand and comply with the license agreements of MindStudio Insight and the third-party software included in the image.

As with all container images, pre-installed software packages such as Python and system libraries may be subject to their own license terms.
