# Installation Description and Preparing Software Packages

## Installation Description

MindStudio Insight is a visual tuning tool for developers. It presents profile data in easy-to-understand charts such as timing diagrams and heatmaps, helping developers quickly identify performance bottlenecks and complete performance optimization. This document mainly describes how to install MindStudio Insight.

MindStudio Insight can be used in the following scenarios:

- Supported on Windows, Linux, and macOS systems;<br>
- Supported as a JupyterLab plugin;<br>
- Supported for direct access by deploying a Docker image.

## Preparing Software Packages

### Package Download

You can select the corresponding software package for your operating system to download. By downloading the binary software, you agree to the terms and conditions of the [Software License Agreement 2.0](https://www.hiascend.com/en/legal/softlicense).

Click [MindStudio Insight Release](https://gitcode.com/Ascend/msinsight/releases), confirm the version information, and obtain the packages listed in [**Table 1** Package list](#package-list).

> [!NOTE]
>
> The `{version}` in the package names in the table is a version number placeholder. Replace it with the current release version number when downloading. For version number details, see [Release Notes](https://gitcode.com/Ascend/msinsight/releases).

**Table 1** Package list<a id="package-list"></a>

| Package                                                      | Description                                                  |
|--|--|
| MindStudio-Insight_*{version}*_win.exe                       | MindStudio Insight package for Windows, an integrated development environment with a GUI. |
| MindStudio-Insight_*{version}*_linux-*{arch}*.zip            | MindStudio Insight package for Linux.                        |
| MindStudio-Insight_*{version}*_macos-*{arch}*.dmg            | MindStudio Insight package for macOS, an integrated development environment with a GUI. |
| mindstudio_insight_jupyterlab-*{version}*-py3-none-*{platform}*.whl | Package for installation based on JupyterLab.                |
|MindStudio-Insight_docker_image_*{version}*-*{os}*_*{Python_version}*_*{arch}*.tar|Docker image package of MindStudio Insight.|

### Software Integrity Verification

To prevent software packages from being maliciously tampered with during transmission or storage, download the corresponding .sha256 file for integrity verification when downloading a software package.

Click [MindStudio Insight Release](https://gitcode.com/Ascend/msinsight/releases) to obtain the hash verification file (.sha256) for the corresponding software package, and perform integrity verification on the [downloaded software package](#package-download). If the verification fails, do not use the software package. For support and services, seek help on the forum or submit a technical ticket.

The specific verification method is as follows:

1. Obtain the SHA256 checksum of the software package locally.

    On Windows, run the following command to obtain the SHA256 checksum of the corresponding software package:

    ```powershell
    certutil -hashfile Software_package_name SHA256
    ```

    On macOS, run the following command to obtain the SHA256 checksum of the corresponding software package:

    ```shell
    shasum -a 256 Software_package_name
    ```

    On Linux, run the following command to obtain the SHA256 checksum of the corresponding software package:

    ```bash
    sha256sum Software_package_name
    ```

2. Open the corresponding hash verification file and compare the checksum in it with the obtained checksum (case-insensitive). If they match, the software package has passed the integrity verification.
