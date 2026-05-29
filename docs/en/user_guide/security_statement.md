# **MindStudio Insight Security Statement**

## System Security Hardening

- MindStudio Insight is a debugging tool and should not be used in the production environment.
- To ensure security, you are advised to use MindStudio Insight as a whole. If secondary development is required, pay attention to and handle the possible security risks.
- MindStudio Insight is a tool for development. You are advised to start MindStudio Insight locally instead of using it through *X* protocol forwarding.
- Except for usage scenarios involving installation via the JupyterLab plugin, MindStudio Insight is a local tool and is secure by default. If opening ports is required, be aware of the security risks associated with remote communication. For details about the security hardening suggestions for the JupyterLab plugin, see section "Communication Security Hardening".

## User Account Recommendations

For security purposes, do not start MindStudio Insight as the **root** user on Linux or macOS. If you must start MindStudio Insight as the root user, run the `./MindStudio-Insight --allow-root` command to start it after ensuring that the environment is secure.

## File Permission Control

In Linux or Unix, when you use the JupyterLab plugin to install MindStudio Insight, check the umask setting of the current user in the environment before installing the software package. You are advised to set the umask to **0027** or stricter to ensure that the files installed using pip are not writable by other users and users in the same group, preventing potential security risks.

**File Permission Reference**

| Type                              | Maximum Linux Permission|
| ---------------------------------- | ------------------- |
| Home directory                        | 750 (rwxr-x---)   |
| Program files (including scripts and library files)    | 550 (r-xr-x---)   |
| Program file directory                      | 550 (r-xr-x---)   |
| Configuration files                          | 640 (rw-r-----)   |
| Configuration file directory                      | 750 (rwxr-x---)   |
| Log files (recorded or archived)    | 440 (r--r-----)   |
| Log files (being recorded)                | 640 (rw-r-----)   |
| Log file directory                      | 750 (rwxr-x---)   |
| Debug files                         | 640 (rw-r-----)   |
| Debug file directory                     | 750 (rwxr-x---)   |
| Temporary file directory                      | 750 (rwxr-x---)   |
| Maintenance and upgrade file directory                  | 770 (rwxrwx---)   |
| Service data files                      | 640 (rw-r-----)   |
| Service data file directory                  | 750 (rwxr-x---)   |
| Key components, private keys, certificates, and ciphertext file directory| 700 (rwx------)   |
| Key components, private keys, certificates, and ciphertext files    | 600 (rw-------)   |
| APIs and scripts for encryption and decryption            | 500 (r-x------)   |

## Vulnerability Security Statement

- Huawei's regulations on product vulnerability management are subject to "Vul. Response Process." For details, see [Vul. Response Process](https://www.huawei.com/en/psirt/vul-response-process).
- For enterprise customers who need vulnerability information, see [Security Bulletins](https://securitybulletin.huawei.com/enterprise/en/security-advisory).
- When installing dependencies for Windows/macOS/Linux, use a newer software package version that meets the requirements. Monitor and patch existing vulnerabilities, especially disclosed high-risk vulnerabilities with a CVSS score greater than 7.

## Data Security Statement

MindStudio Insight supports importing various types of tuning data (including but not limited to those collected by PyTorch Memory Snapshot, PyTorch Profiler, msprof, memscope, msServiceProfiler, and other tools; for details, refer to [overview](./overview.md)). Before importing, ensure that the data source is trustworthy and the environment is secure.
 

## Running Security Statement

MindStudio Insight will exit the process and print an error message when a runtime exception occurs. It is recommended to identify the specific cause of the error based on the error prompt. The error information covers aspects such as file permissions, file parsing, data writing to drives, and data querying.

## Communication Security Hardening

- MindStudio Insight, when installed and used via the JupyterLab plugin, operates in a secure usage mode by default. In this mode, the JupyterLab service is launched on the same machine as the web browser—meaning it is accessed via a local browser on the machine's Linux system. Communication is securely established over localhost.
- When MindStudio Insight is installed and used via the JupyterLab plugin, remote access is considered an insecure usage mode. In this mode, the JupyterLab service is started on a different machine than the one running the web browser. To enable remote access, users must either modify the JupyterLab configuration file or install the jupyter_server_proxy plugin to proxy the required port. This allows access from a remote machine but results in insecure cross-machine communication. Users are responsible for assessing and mitigating the security risks associated with remote communication.
- To mitigate security risks, access control policies such as iptables can be configured on the client where the JupyterLab plugin resides, or reverse proxy tools like nginx can be used to harden HTTPS security. In addition, users can modify the JupyterLab configuration file to harden the security of the browser protection mechanism of JupyterLab.

## Communication matrix

**Contact Information**

| No.| Source Device| Source IP Address| Source Port| Destination Device| Destination IP Address| Destination Port (Listening)| Protocol| Port Description| Listening Port Configurable (Yes/No)| Authentication Mode| Encryption Mode| Plane| Version| Special Scenarios| Remarks|
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | PC where MindStudio Insight is installed| IP address of the client on the PC where MindStudio Insight is installed| Random port allocated by the system| <https://support.huawei.com/ecolumnsweb/en/warranty-policy> server| <https://support.huawei.com/ecolumnsweb/en/warranty-policy> | 443 | HTTPS | [Function] Website in the EULA description. [Note] During the installation, you can click this link to go to the corresponding address. During the installation of MindStudio Insight, the EULA license is displayed to the user. When the user views the license, the user can click this link to automatically open the browser and go to the Huawei official product end-of-life policy.| N/A| N/A| SSL | None| >=7.0.RC1 | None| None|
| 2 | PC where MindStudio Insight is installed| IP address of the client on the PC where MindStudio Insight is installed| Random port allocated by the system| <https://www.huawei.com/en/psirt/vul-response-process> server| <https://www.huawei.com/en/psirt/vul-response-process> | 443 | HTTPS | [Function] Website in the EULA description. [Note] During the installation, you can click this link to go to the corresponding address. During the installation of MindStudio Insight, the EULA license is displayed to the user. When the user views the license, the user can click this link to automatically open the browser and go to the Huawei official vulnerability management policy.| N/A| N/A| SSL | None| >=7.0.RC1 | None| None|
| 3 | PC where MindStudio Insight is installed| IP address of the client on the PC where MindStudio Insight is installed| Random port allocated by the system| <https://support.huawei.com/additionalres/pki> server| <https://support.huawei.com/additionalres/pki> | 443 | HTTPS | [Function] During the installation of MindStudio Insight, the EULA license is displayed to the user. When the user views the license, the user can click this link to automatically open the browser and go to the Huawei official website to download the root certificate and sub-certificates.| N/A| N/A| SSL | None| >=7.0.RC1 | None| None|
| 4 | PC where MindStudio Insight is installed| 127.0.0.1 | Random port allocated by the system| Local device| 127.0.0.1 | 9000~9100 | TCP | This port is enabled for the MindStudio Insight backend service to communicate with the frontend. An idle port among the first 100 ports starting from the start port is selected.| N/A| N/A| None| None| >=7.0.RC1 | None| None|
