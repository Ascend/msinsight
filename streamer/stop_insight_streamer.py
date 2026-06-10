#!/usr/bin/env python3
# pylint: disable=duplicate-code
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
# Standalone Python script to stop local MindStudio Insight Docker containers.
# It can match containers by name, exposed port, or the msinsight image repository.

import argparse
import json
import logging
import shutil
import subprocess  # nosec B404
import sys

DEFAULT_CONTAINER_NAME = "msinsight-streamer"
IMAGE_REPOSITORY_NAME = "msinsight"
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")


def get_docker_command():
    """Return the absolute Docker executable path to avoid PATH lookup surprises."""
    docker_command = shutil.which("docker")
    if not docker_command:
        logging.error("Docker executable was not found in PATH.")
        sys.exit(1)
    return docker_command


def run_command(command, check=True):
    """Run a command and capture stdout/stderr for parsing or error reporting."""
    return subprocess.run(command, capture_output=True, text=True, check=check)  # nosec B603


def image_matches(image):
    """Return True when a running container uses an msinsight image repository."""
    repository = image.split(":", 1)[0].lower().split("/")[-1]
    return repository == IMAGE_REPOSITORY_NAME


def get_docker_containers():
    """Return running Docker containers with the fields needed for matching and display."""
    try:
        result = run_command([get_docker_command(), "ps", "--format", "{{json .}}"])
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        logging.error("failed to list Docker containers: %s", exc)
        sys.exit(1)

    containers = []
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        data = json.loads(line)
        containers.append(
            {
                "id": data.get("ID", ""),
                "name": data.get("Names", ""),
                "image": data.get("Image", ""),
                "ports": data.get("Ports", ""),
            }
        )
    return containers


def find_matching_containers(container_name=None, port=None, all_insight=False):
    """Find containers by explicit name, exposed host port, or msinsight image repository."""
    matches = []
    for container in get_docker_containers():
        if container_name and container["name"] == container_name:
            matches.append(container)
            continue
        if port and f":{port}->" in container["ports"]:
            matches.append(container)
            continue
        if all_insight and image_matches(container["image"]):
            matches.append(container)
            continue
        if not container_name and not port and container["name"] == DEFAULT_CONTAINER_NAME:
            matches.append(container)
    return matches


def stop_container(container_name):
    """Stop a container, first renaming it to release the original name quickly."""
    removal_name = f"{container_name}-removal"

    # Renaming first frees the original container name immediately, which makes stop/start loops faster.
    # If rename fails, fall back to stopping the original name instead of blindly stopping '*-removal'.
    rename_result = subprocess.run(
        [get_docker_command(), "rename", container_name, removal_name],
        capture_output=True,
        text=True,
        check=False,
    )  # nosec B603

    if rename_result.returncode == 0:
        target_name = removal_name
    else:
        logging.warning("failed to rename container '%s' to '%s'.", container_name, removal_name)
        if rename_result.stderr:
            logging.warning("%s", rename_result.stderr.strip())
        target_name = container_name

    subprocess.run([get_docker_command(), "stop", target_name], check=True)  # nosec B603
    logging.info("Container '%s' stopped successfully.", container_name)


def parse_args():
    """Parse stop filters. Without filters, the default container name is used."""
    parser = argparse.ArgumentParser(
        description="Stop local MindStudio Insight Docker containers by name, port, or msinsight image repository."
    )
    parser.add_argument(
        "-n", "--container-name", default=None, help=f"Container name to stop. Defaults to {DEFAULT_CONTAINER_NAME}."
    )
    parser.add_argument("-p", "--port", type=str, default=None, help="Stop the container exposing this host port.")
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help=f"Stop all running containers whose image repository is {IMAGE_REPOSITORY_NAME}.",
    )
    parser.add_argument("-y", "--yes", action="store_true", help="Automatically confirm stopping containers.")
    return parser.parse_args()


def main():
    args = parse_args()
    matches = find_matching_containers(container_name=args.container_name, port=args.port, all_insight=args.all)

    if not matches:
        target = args.container_name or (f"port {args.port}" if args.port else DEFAULT_CONTAINER_NAME)
        logging.info("No running Insight container found for %s.", target)
        return

    # The same container can match more than once, for example by both name and port. Stop it only once.
    seen = set()
    for container in matches:
        name = container["name"]
        if name in seen:
            continue
        seen.add(name)

        if not args.yes:
            answer = (
                input(
                    f"Container '{name}' with image '{container['image']}' is running on ports '{container['ports']}'. "
                    "Stop it? (yes/no): "
                )
                .strip()
                .lower()
            )
            if answer not in {"y", "yes"}:
                logging.info("Container '%s' not stopped.", name)
                continue

        stop_container(name)


if __name__ == "__main__":
    main()
