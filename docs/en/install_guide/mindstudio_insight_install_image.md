# Installing, Upgrading, and Uninstalling MindStudio Insight by Using an Image

## Installing MindStudio Insight

<a id="installation-operation-image"></a>

### Installation Operation (Image)

**Environment Preparation**

1. Prepare a Linux server with Docker installed and the Docker service running properly.
2. Select an image acquisition method based on the server's network environment:

    - Online acquisition: Ensure that the server can access the AscendHub image repository.
    - Offline acquisition: Download the MindStudio Insight image package corresponding to the server's CPU architecture, and complete the [Software Integrity Verification](./mindstudio_insight_install_overview.md#software-integrity-verification).

3. Ensure that the client can access the mapped port for MindStudio Insight on the server.

> [!NOTE]
>
> The operating system and Python runtime environment are pre-installed in the image, so you do not need to install MindStudio Insight runtime dependencies on the server separately. If you need to build an image yourself, see [MindStudio Insight Image Description](https://gitcode.com/Ascend/msinsight/blob/master/docker/OVERVIEW.md#local-build).

**Obtaining the Image**

Depending on the network environment of the server, you can pull the image online from the AscendHub image repository or load a downloaded offline image package.

<a id="online-pull"></a>

#### Method 1: Pulling the Image Online

1. Visit the [AscendHub MindStudio Insight Image Page](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1) and select an image tag based on the base operating system and Python version of the image.

    The following image tags are the latest, all supporting both `x86_64` and `aarch64` architectures.

    |Image Tag|Base Operating System|Python Version|
    |--|--|--|
    | [`26.2.0-ubuntu22.04-py3.10`](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1?version=26.2.0-ubuntu22.04-py3.10) |Ubuntu 22.04|3.10|
    | [`26.2.0-openeuler24.03-py3.11`](https://www.hiascend.com/developer/ascendhub/detail/e40957c8eb4245e3a189b818b2408eb1?version=26.2.0-openeuler24.03-py3.11) |openEuler 24.03 LTS|3.11|

2. In the operation column of the corresponding tag on the image page, click **Download**, and follow the steps displayed on the page to copy and execute the image pull command.

    > [!NOTE]
    >
    > The image address and pull command in the AscendHub image repository may change with repository configuration. Refer to the information displayed in real time on the image page.

3. Run the following command to confirm that the image has been pulled successfully:

    ```shell
    docker images
    ```

    In the echo information, locate the pulled MindStudio Insight image and record the image name and tag. The image is represented by `{image_name}:{image_tag}` in the following text.

<a id="loading-an-offline-image"></a>

#### Method 2: Loading an Offline Image

1. Upload the downloaded image package to the server.
2. In the directory where the image package is located, run the following command to load the image:

    ```shell
    docker load -i MindStudio-Insight_docker_image_{version}-{os}_{Python_version}_{arch}.tar
    ```

    The command echo information displays the name and tag of the loaded image, for example:

    ```text
    Loaded image: {image_name}:{image_tag}
    ```

3. Run the following command to confirm that the image has been loaded successfully:

    ```shell
    docker images
    ```

    Locate the loaded MindStudio Insight image in the echo information, and record the image name and tag. The image is represented by `{image_name}:{image_tag}` in the following text.

**Running the Image**

Choose either HTTPS + mTLS mode or HTTP mode to run the MindStudio Insight image based on your usage scenario. HTTPS + mTLS mode is recommended for production environments or multi-user shared service scenarios.

It is recommended to use the MindStudio Insight Streamer script to control container startup and shutdown. The script can automatically complete image selection, port mapping, and mounting of data and certificate directories, and automatically remove the container object after the container stops. You can also directly use Docker commands to run the image as needed.

<a id="using-the-streamer-script-to-run-the-image"></a>

#### Method 1: Using the Streamer Script to Run the Image (Recommended)

1. Obtain the `streamer` directory from the [MindStudio Insight Code Repository](https://gitcode.com/Ascend/msinsight) and navigate to this directory on the server.
2. Select an image based on the image name after obtaining the image:

    - If the image repository name is `msinsight`, the script automatically selects the latest local `msinsight` image, and you do not need to specify `--image`.
    - If the image repository name is not `msinsight`, specify the image using `--image {image_name}:{image_tag}` when executing the script.

3. Start MindStudio Insight based on the usage scenario.

    - HTTPS + mTLS mode (recommended)

        1. Prepare the server certificate, server private key, and CA certificate, and place the certificate files in the same directory on the server. The file names must be as follows:

            ```text
            server.crt
            server.key
            ca.crt
            ```

            > [!NOTE]
            >
            > Properly set the access permissions for the certificate directory and private key files to prevent certificate or private key leakage. The client must also be configured with a client certificate issued by this CA to complete mTLS mutual authentication.

        2. Start the container.

            ```shell
            python3 run_insight_streamer.py \
              --image {image_name}:{image_tag} \
              --cert-dir /path/to/certs \
              -v /path/to/profile_data
            ```

            If the image repository name is `msinsight`, you can omit `--image`. By default, the script maps the server's `8443` port to the container's HTTPS port. If the default port is already occupied, you can add `--https-port dynamic` to automatically select an available port, or specify a port using `--https-port {host_https_port}`.

        3. Based on the URL in the command echo information, open MindStudio Insight in a browser that has the client certificate configured. The default access address is as follows:

            ```text
            https://{host_ip}:8443/?proxy=true
            ```

    - HTTP mode (applicable only to development, testing, or temporary access)

        1. Start the container.

            ```shell
            python3 run_insight_streamer.py \
              --image {image_name}:{image_tag} \
              -v /path/to/profile_data
            ```

            If the image repository name is `msinsight`, you can omit `--image` . By default, the script maps the server's `8080` port to the container's HTTP port. If the default port is already occupied, you can add `--http-port dynamic` to automatically select an available port, or specify a port using `--http-port {host_http_port}`.

        2. Open MindStudio Insight in a browser using the URL displayed in the command echo. The default access address is as follows:

            ```text
            http://{host_ip}:8080/?proxy=true
            ```

        > [!WARNING] WARNING
        > The HTTP mode does not provide transport layer encryption or client authentication. Do not use it in production environments or untrusted networks.

4. To stop the default container started by the Streamer script, run the following command in the `streamer` directory:

    ```shell
    python3 stop_insight_streamer.py
    ```

    For more information about image specification, port configuration, container stopping, and Docker parameter passthrough, see [MindStudio Insight Streamer Script Description](https://gitcode.com/Ascend/msinsight/blob/master/streamer/README.md).

<a id="using-docker-commands-to-run-an-image"></a>

#### Method 2: Using Docker Commands to Run an Image

- HTTPS + mTLS mode (recommended)

    1. Prepare certificates as required in [Method 1](#using-the-streamer-script-to-run-the-image).
    2. Start the container.

        ```shell
        docker run -d \
          -p {host_https_port}:443 \
          -v /path/to/profile_data:/opt/insight/data \
          -v /path/to/certs:/etc/nginx/certs:ro \
          --name msinsight \
          {image_name}:{image_tag}
        ```

    3. Access the following address in a browser with the client certificate configured to open MindStudio Insight:

        ```text
        https://{host_ip}:{host_https_port}/?proxy=true
        ```

- HTTP mode (applicable only to development, testing, or temporary access)

    1. Start the container.

        ```shell
        docker run -d \
          -p {host_http_port}:80 \
          -v /path/to/profile_data:/opt/insight/data \
          --name msinsight \
          {image_name}:{image_tag}
        ```

    2. Access the following address in a browser to open MindStudio Insight:

        ```text
        http://{host_ip}:{host_http_port}/?proxy=true
        ```

    > [!WARNING] Warning
    > The HTTP mode does not provide transport layer encryption or client identity authentication. Do not use it in production environments or untrusted networks.

The related parameters are described as follows.

|Parameter|Description|
|--|--|
|`{host_https_port}`|HTTPS port on the server for accessing MindStudio Insight, for example, `9443`.|
|`{host_http_port}`|HTTP port on the server for accessing MindStudio Insight, for example, `9880`.|
|`/path/to/profile_data`|Directory on the server where the profile data to be analyzed is stored. Replace it with the actual absolute path.|
|`/path/to/certs`|Directory on the server where the mTLS certificates are stored. Replace it with the actual absolute path.|
|`msinsight`|Container name, which can be modified based on the actual situation.|
|`{image_name}:{image_tag}`|Image name and tag obtained after online pull or offline image loading.|

Run the following command to check whether the container has started successfully:

```shell
docker ps --filter name=msinsight
```

If the container is not running properly, run the following command to view the container logs:

```shell
docker logs msinsight
```

> [!NOTE]
>
> - `{host_ip}` is the IP address of the server running the container. Ensure that the client can communicate with the server over the network and that the server firewall has opened the corresponding mapped ports.
> - The server directory mounted to `/opt/insight/data` can be used in MindStudio Insight to access the profile data to be analyzed.
> - When starting a container using the Docker command, HTTPS + mTLS mode is enabled only if `server.crt`, `server.key`, and `ca.crt` all exist in the `/etc/nginx/certs` directory; otherwise, HTTP mode is enabled.

## Upgrading MindStudio Insight

If you need to upgrade MindStudio Insight, you must first uninstall the currently installed MindStudio Insight, then obtain the latest MindStudio Insight software package and reinstall it.

Based on your actual scenario, refer to [Uninstalling MindStudio Insight](#uninstalling-mindstudio-insight) to complete the uninstallation, and then reinstall the latest MindStudio Insight software package.

## Uninstalling MindStudio Insight

<a id="uninstallation-operations-image"></a>

### Uninstallation Operations (Image)

1. Stop and delete the MindStudio Insight container.

    - If the container was started using the Streamer script, it is recommended to run the following command in the `streamer` directory to stop the default container:

        ```shell
        python3 stop_insight_streamer.py
        ```

        The container started by Streamer is configured with `--rm`, so the container object is automatically deleted after the container stops. If a different container name was specified at startup, you can specify that name using `-n`:

        ```shell
        python3 stop_insight_streamer.py -n {container_name}
        ```

    - If you started the container using a Docker command, run the following command to stop and delete the container:

        ```shell
        docker rm -f {container_name}
        ```

        Where `{container_name}` is the container name specified when starting the container, for example, `msinsight`.

2. Run the following command to confirm that no containers you need to keep are using the image to be deleted:

    ```shell
    docker ps -a --filter ancestor={image_name}:{image_tag}
    ```

3. Run the following command to delete the MindStudio Insight image:

    ```shell
    docker rmi {image_name}:{image_tag}
    ```

    Where `{image_name}:{image_tag}` is the image name and tag recorded when loading the image, which can be queried using `docker images`.

> [!NOTE]
>
> - Deleting containers and images does not delete the server data directory mounted via `-v`. If you confirm that the profile data in it is no longer needed, manually clean it up based on the actual mount path.
> - If `docker rmi` prompts that the image is in use, first identify and delete the associated container based on the echo information, and then re-execute the image deletion command. Do not forcibly delete an image without confirming the purpose of the container.
> - For more methods such as stopping containers by port and stopping all running `msinsight` containers, see [MindStudio Insight Streamer Script Description](https://gitcode.com/Ascend/msinsight/blob/master/streamer/README.md#stop-insight).
