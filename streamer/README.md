# MindStudio Insight Streamer Scripts

This directory contains local Docker helper scripts for running and stopping MindStudio Insight containers.

The scripts focus on the container runtime itself: image discovery, HTTP or HTTPS+mTLS startup, host volume mounting, certificate mounting, port selection, and container shutdown.

## Capabilities

- Auto-discover the newest local `msinsight` image.
- Start Insight in HTTP mode.
- Start Insight in HTTPS + mTLS mode.
- Mount a host data directory into `/opt/insight/data`.
- Mount a host certificate directory into `/etc/nginx/certs`.
- Use default ports, explicit ports, or dynamic ports.
- Pass through additional `docker run` arguments.
- Stop containers by name, host port, or `msinsight` image repository.

## Files

| File | Description |
|---|---|
| `run_insight_streamer.py` | Starts a local MindStudio Insight Docker container. |
| `stop_insight_streamer.py` | Stops local MindStudio Insight Docker containers by name, port, or image repository. |

## Prerequisites

- Docker is installed and running.
- A local MindStudio Insight image is available.
- For auto-discovery, the image repository name should be `msinsight`.

Example image name:

```text
msinsight:26.1.0-ubuntu22.04
```

If no local `msinsight` image is found, download or build one first. Recommended download site:

```text
https://www.hiascend.com/developer/ascendhub
```

You can also explicitly specify any image with `--image`.

## Run Insight

Run the following commands from the `streamer` directory.

### HTTP mode

```bash
python3 run_insight_streamer.py \
  -v /path/to/profile_data
```

This starts a container with the newest local `msinsight` image and maps:

```text
<host_data_dir> -> /opt/insight/data
<host_http_port> -> container port 80
```

Default HTTP port:

```text
8080
```

Access URL:

```text
http://<host_ip>:8080/?proxy=true
```

### HTTPS + mTLS mode

```bash
python3 run_insight_streamer.py \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

The certificate directory must contain:

```text
server.crt
server.key
ca.crt
```

The script mounts the certificate directory to the fixed container path:

```text
/etc/nginx/certs
```

This matches the nginx and entrypoint configuration in the image.

Default HTTPS port:

```text
8443
```

Access URL:

```text
https://<host_ip>:8443/?proxy=true
```

Health check example:

```bash
curl -k \
  --cert /path/to/client.crt \
  --key /path/to/client.key \
  https://<host_ip>:8443/health
```

## Parameters

### `run_insight_streamer.py`

| Parameter | Default | Description |
|---|---|---|
| `--image` | Newest local `msinsight` image | Image to run. If omitted, the script scans local images whose repository basename is `msinsight` and picks the newest tag. |
| `-n`, `--container-name` | `msinsight-streamer` | Docker container name. If a container with the same name already exists, use `-y` to replace it without confirmation. |
| `--host-ip` | Auto-detected host IP | IP address printed in the final access URL. It does not change Docker port binding. |
| `--http-port` | `8080` | Host port used in HTTP mode. Set to `dynamic` to try the default port first and then choose a free port if needed. |
| `--https-port` | `8443` | Host port used in HTTPS + mTLS mode. Set to `dynamic` to choose a free port if needed. |
| `-v`, `--volume` | None | Host data directory. Recommended. It is mounted to `/opt/insight/data` in the container. Can be specified multiple times. |
| `--data-mount` | `/opt/insight/data` | Container data mount path. Usually keep the default unless Insight is configured to use another data directory. |
| `--cert-dir` | None | Host certificate directory. When set, the script enables HTTPS + mTLS and mounts it to `/etc/nginx/certs`. |
| `--mode` | `auto` | Run mode: `auto`, `http`, or `mtls`. In `auto` mode, mTLS is enabled when `--cert-dir` is provided; otherwise HTTP is used. |
| `-y`, `--yes` | `false` | Automatically replace an existing container with the same name. |
| Arguments after `--` | None | Extra native Docker arguments passed directly to `docker run`, for example `-- --cpus 2 --memory 4g`. |

### `stop_insight_streamer.py`

| Parameter | Default | Description |
|---|---|---|
| `-n`, `--container-name` | `msinsight-streamer` | Stop a container by name. |
| `-p`, `--port` | None | Stop the container exposing the specified host port, for example `-p 8443`. |
| `-a`, `--all` | `false` | Stop all running containers whose image repository basename is `msinsight`. |
| `-y`, `--yes` | `false` | Confirm stop operations automatically without prompting. |

## Common Examples

### Specify an image

```bash
python3 run_insight_streamer.py \
  --image msinsight:26.1.0-ubuntu22.04 \
  -v /path/to/profile_data
```

### Specify HTTPS port

```bash
python3 run_insight_streamer.py \
  --image msinsight:26.1.0-ubuntu22.04 \
  --https-port 9880 \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

Access URL:

```text
https://<host_ip>:9880/?proxy=true
```

### Dynamic port

HTTP mode:

```bash
python3 run_insight_streamer.py \
  --http-port dynamic \
  -v /path/to/profile_data
```

HTTPS + mTLS mode:

```bash
python3 run_insight_streamer.py \
  --https-port dynamic \
  --cert-dir /path/to/certs \
  -v /path/to/profile_data
```

The script first tries the default port and then asks the OS for a free port if the default is unavailable.

### Pass additional Docker arguments

Arguments after `--` are passed directly to `docker run`:

```bash
python3 run_insight_streamer.py \
  -v /path/to/profile_data \
  -- --cpus 2 --memory 4g
```

## Data Directory Mounting

The `-v` option takes a host data directory, not full Docker `host:container` syntax.

Example:

```bash
-v /home/user/profile_data
```

The script converts it to:

```text
-v /home/user/profile_data:/opt/insight/data
```

If multiple `-v` options are provided, the first one is mounted to `/opt/insight/data`; later ones use suffixed mount paths such as `/opt/insight/data-1`.

## Stop Insight

### Stop the default container

```bash
python3 stop_insight_streamer.py
```

### Stop without confirmation

```bash
python3 stop_insight_streamer.py -y
```

### Stop by container name

```bash
python3 stop_insight_streamer.py -n msinsight-streamer
```

### Stop by exposed host port

```bash
python3 stop_insight_streamer.py -p 8443
```

### Stop all running msinsight containers

```bash
python3 stop_insight_streamer.py -a
```

## Container Lifecycle

The run script starts containers with:

```text
--rm
```

Therefore, when a container is stopped, Docker automatically removes it. Data remains available if it is stored in a host directory mounted with `-v`.

To reopen Insight after stopping it, run `run_insight_streamer.py` again with the same image, data directory, certificate directory, and port options.

## Notes

- Automatic image discovery only checks images whose repository basename is `msinsight`.
- Use `--image` if the image name is different.
- HTTP mode is suitable for local validation only.
- HTTPS + mTLS requires `server.crt`, `server.key`, and `ca.crt` in the certificate directory.
- The certificate mount path is fixed to `/etc/nginx/certs` because the image entrypoint and nginx configuration use this path.
