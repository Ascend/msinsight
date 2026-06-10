#!/usr/bin/env python3
# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------
#
# Standalone Python script to run a local MindStudio Insight Docker container.
# It discovers local msinsight images, mounts data/cert directories, and prints access URLs.

import argparse
import json
import logging
import os
import re
import shutil
import socket
import subprocess  # nosec B404
import sys

# Download page shown when no local msinsight image is available.
ASCENDHUB_URL = "https://www.hiascend.com/developer/ascendhub"

# Only images whose repository basename is exactly "msinsight" are auto-discovered.
# Users can still run another image explicitly with --image.
IMAGE_REPOSITORY_NAME = "msinsight"

# Local Docker defaults. HTTP is for quick testing; HTTPS+mTLS is enabled by --cert-dir.
DEFAULT_HTTP_PORT = 8080
DEFAULT_HTTPS_PORT = 8443
DEFAULT_DATA_MOUNT = "/opt/insight/data"
DEFAULT_CERT_MOUNT = "/etc/nginx/certs"
DEFAULT_CONTAINER_NAME = "msinsight-streamer"

# The image entrypoint and nginx.conf both use these exact filenames under /etc/nginx/certs.
REQUIRED_CERT_FILES = ("server.crt", "server.key", "ca.crt")
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")


def get_docker_command():
    """Return the absolute Docker executable path to avoid PATH lookup surprises."""
    docker_command = shutil.which("docker")
    if not docker_command:
        logging.error("Docker executable was not found in PATH.")
        sys.exit(1)
    return docker_command


def run_command(command, check=True):
    """Run a command and capture stdout/stderr for later parsing or error reporting."""
    return subprocess.run(command, capture_output=True, text=True, check=check)  # nosec B603


def parse_version_parts(tag):
    """Extract numeric parts from an image tag so tags can be sorted approximately by version."""
    numbers = [int(part) for part in re.findall(r"\d+", tag)]
    if not numbers:
        return (-1, tag)
    return (*numbers, tag)


def image_matches(repository):
    """Return True when the image repository basename matches the official msinsight name."""
    return repository.split("/")[-1] == IMAGE_REPOSITORY_NAME


def list_local_images():
    """List local Docker images that look like MindStudio Insight images."""
    try:
        result = run_command([get_docker_command(), "images", "--format", "{{json .}}"])
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        logging.error("failed to list Docker images: %s", exc)
        sys.exit(1)

    images = []
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        data = json.loads(line)
        repository = data.get("Repository", "")
        tag = data.get("Tag", "")
        if repository == "<none>" or tag == "<none>":
            continue
        if image_matches(repository):
            images.append({"repository": repository, "tag": tag, "image": f"{repository}:{tag}"})
    return images


def find_latest_image():
    """Pick the newest local msinsight image based on numeric tag components."""
    images = list_local_images()
    if not images:
        return None
    return sorted(images, key=lambda item: parse_version_parts(item["tag"]), reverse=True)[0]


def get_default_host_ip():
    """Return the host IP printed in the final browser URL."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # The address does not need to be reachable; this only asks the OS which source IP it would use.
        sock.connect(("8.8.8.8", 80))
        return sock.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        sock.close()


def is_port_available(port):
    """Check whether a local TCP port is available for Docker to bind."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        return sock.connect_ex(("127.0.0.1", int(port))) != 0


def find_available_port(preferred_port):
    """Use the preferred port if possible; otherwise ask the OS for an available ephemeral port."""
    if is_port_available(preferred_port):
        return preferred_port
    sock = socket.socket()
    sock.bind(("", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def check_cert_dir(cert_dir):
    """Validate that the host certificate directory can trigger HTTPS+mTLS inside the image."""
    if not cert_dir:
        return False
    missing = [name for name in REQUIRED_CERT_FILES if not os.path.isfile(os.path.join(cert_dir, name))]
    if missing:
        logging.error("certificate directory '%s' is missing: %s", cert_dir, ", ".join(missing))
        sys.exit(1)
    return True


def container_exists(container_name):
    """Return True if Docker already has a container with the requested name."""
    result = run_command([get_docker_command(), "ps", "-a", "--format", "{{.Names}}"], check=False)
    return container_name in result.stdout.splitlines()


def stop_existing_container(container_name, auto_yes):
    """Remove an existing container before starting a new one with the same name."""
    if not container_exists(container_name):
        return
    if not auto_yes:
        answer = input(f"Container '{container_name}' already exists. Stop and remove it? (yes/no): ").strip().lower()
        if answer not in {"y", "yes"}:
            logging.info("Existing container kept. Exiting.")
            sys.exit(1)
    subprocess.run([get_docker_command(), "rm", "-f", container_name], check=True)  # nosec B603


def parse_args():
    """Parse script arguments and preserve raw Docker arguments after '--'."""
    parser = argparse.ArgumentParser(
        description="Run a local Docker container for MindStudio Insight.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=(
            "Examples:\n"
            "  run_insight_streamer.py\n"
            "  run_insight_streamer.py -v /path/to/profile_data\n"
            "  run_insight_streamer.py --cert-dir docker/certs -v /path/to/profile_data\n"
            "  run_insight_streamer.py --image msinsight:26.1.0-ubuntu22.04 --https-port 8443 --cert-dir /path/to/certs\n"
            "\nAdditional docker arguments may be passed after '--'.\n"
            "  run_insight_streamer.py -v /data -- --cpus 2 --memory 4g"
        ),
    )
    parser.add_argument("--image", help="Image to run. If omitted, the newest local msinsight image tag is used.")
    parser.add_argument(
        "-n",
        "--container-name",
        default=DEFAULT_CONTAINER_NAME,
        help=f"Container name (default: {DEFAULT_CONTAINER_NAME}).",
    )
    parser.add_argument("--host-ip", default=get_default_host_ip(), help="Host IP printed in the final access URL.")
    parser.add_argument(
        "--http-port", default=str(DEFAULT_HTTP_PORT), help="Host HTTP port. Use 'dynamic' to choose a free port."
    )
    parser.add_argument(
        "--https-port", default=str(DEFAULT_HTTPS_PORT), help="Host HTTPS port. Use 'dynamic' to choose a free port."
    )
    parser.add_argument(
        "-v", "--volume", action="append", help=f"Host data directory mounted to {DEFAULT_DATA_MOUNT}. Recommended."
    )
    parser.add_argument(
        "--data-mount", default=DEFAULT_DATA_MOUNT, help=f"Container data mount path (default: {DEFAULT_DATA_MOUNT})."
    )
    parser.add_argument(
        "--cert-dir", help=f"Certificate directory mounted read-only to {DEFAULT_CERT_MOUNT}. Enables HTTPS + mTLS."
    )
    parser.add_argument(
        "--mode",
        choices=("auto", "http", "mtls"),
        default="auto",
        help="Run mode. auto enables mTLS when --cert-dir is provided.",
    )
    parser.add_argument(
        "-y", "--yes", action="store_true", help="Automatically replace an existing container with the same name."
    )

    # Everything after '--' is intentionally not interpreted by this script and is passed to docker run.
    if "--" in sys.argv:
        index = sys.argv.index("--")
        args = parser.parse_args(sys.argv[1:index])
        args.docker_args = sys.argv[index + 1 :]
    else:
        args = parser.parse_args()
        args.docker_args = []
    return args


def resolve_image(user_image):
    """Use the user-provided image, or auto-discover the newest local msinsight image."""
    if user_image:
        return user_image

    image = find_latest_image()
    if image:
        logging.info("Using newest local image: %s", image["image"])
        return image["image"]

    logging.error("no local MindStudio Insight Docker image was found.")
    logging.error("Expected a local image repository named '%s', for example:", IMAGE_REPOSITORY_NAME)
    logging.error("  msinsight:26.1.0-ubuntu22.04")
    logging.error("Please download or build the image first. Recommended download site: %s", ASCENDHUB_URL)
    sys.exit(1)


def resolve_port(port, default_port):
    """Resolve a fixed port or the special 'dynamic' value."""
    if str(port).lower() == "dynamic":
        return find_available_port(default_port)
    try:
        port = int(port)
    except ValueError:
        logging.error("invalid port: %s", port)
        sys.exit(1)
    if not is_port_available(port):
        logging.error("port %s is not available. Use --http-port dynamic or --https-port dynamic.", port)
        sys.exit(1)
    return port


def build_volume_args(volumes, data_mount):
    """Convert user -v host directories into Docker -v host:container mappings."""
    args = []
    if not volumes:
        logging.warning("no data directory is mounted. It is recommended to use '-v <host_data_dir>'.")
        return args
    if len(volumes) > 1:
        logging.warning(
            "multiple -v values were provided; all will be mounted under the same container data path with suffixes."
        )
    for index, host_path in enumerate(volumes):
        host_path = os.path.abspath(os.path.expanduser(host_path))
        if not os.path.exists(host_path):
            os.makedirs(host_path, exist_ok=True)
        # The first data directory uses the image's expected data path. Additional directories get suffixes.
        mount_path = data_mount if index == 0 else f"{data_mount}-{index}"
        args.extend(["-v", f"{host_path}:{mount_path}"])
    return args


def run_docker_container(
    container_name, image, http_port=None, https_port=None, volume_args=None, cert_dir=None, docker_args=None
):
    """Build and execute the docker run command from explicit high-level parameters."""
    docker_cmd = [get_docker_command(), "run", "-d", "--rm", "--name", container_name]

    # The image listens on 80 in HTTP mode and on 443 in HTTPS+mTLS mode.
    if https_port is not None:
        docker_cmd.extend(["-p", f"{https_port}:443"])
    elif http_port is not None:
        docker_cmd.extend(["-p", f"{http_port}:80"])

    if volume_args:
        docker_cmd.extend(volume_args)

    # The container entrypoint and nginx.conf both require certificates at /etc/nginx/certs.
    if cert_dir:
        docker_cmd.extend(["-v", f"{cert_dir}:{DEFAULT_CERT_MOUNT}:ro"])

    if docker_args:
        docker_cmd.extend(docker_args)

    docker_cmd.append(image)

    logging.info("Running Docker container with command:")
    logging.info("%s", " ".join(docker_cmd))
    subprocess.run(docker_cmd, check=True)  # nosec B603


def main():
    args = parse_args()
    image = resolve_image(args.image)

    # auto mode keeps HTTP as the simple default and switches to mTLS when certificates are provided.
    mtls_enabled = args.mode == "mtls" or (args.mode == "auto" and args.cert_dir)
    if args.mode == "mtls" and not args.cert_dir:
        logging.error("--cert-dir is required when --mode=mtls.")
        sys.exit(1)
    if args.cert_dir:
        args.cert_dir = os.path.abspath(os.path.expanduser(args.cert_dir))
        check_cert_dir(args.cert_dir)

    http_port = None
    https_port = None
    if mtls_enabled:
        https_port = resolve_port(args.https_port, DEFAULT_HTTPS_PORT)
    else:
        http_port = resolve_port(args.http_port, DEFAULT_HTTP_PORT)

    stop_existing_container(args.container_name, args.yes)

    run_docker_container(
        container_name=args.container_name,
        image=image,
        http_port=http_port,
        https_port=https_port,
        volume_args=build_volume_args(args.volume, args.data_mount),
        cert_dir=args.cert_dir if mtls_enabled else None,
        docker_args=args.docker_args,
    )

    if mtls_enabled:
        logging.info("Browse to https://%s:%s/?proxy=true", args.host_ip, https_port)
        logging.info(
            "mTLS is enabled. Import/use a client certificate signed by ca.crt before accessing from a browser."
        )
        logging.info(
            "Health check example: curl -k --cert <client.crt> --key <client.key> https://%s:%s/health",
            args.host_ip,
            https_port,
        )
    else:
        logging.info("Browse to http://%s:%s/?proxy=true", args.host_ip, http_port)
        logging.info("Health check example: curl http://%s:%s/health", args.host_ip, http_port)


if __name__ == "__main__":
    main()
