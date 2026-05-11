# **MindStudio Insight安全声明**

## 系统安全加固

- MindStudio Insight为开发调试工具，不应在生产环境使用。
- 为确保安全性，建议整包使用MindStudio Insight工具。如果需要二次开发，请自行关注并处理可能引入的安全风险。
- MindStudio Insight为开发阶段工具，不建议通过X协议转发进行使用，建议在本地启动使用。
- 除通过JupyterLab插件安装使用场景外，MindStudio Insight为本地工具，默认安全，如果需要打开端口，请注意远程通信带来的安全风险。JupyterLab插件的安全加固建议详见“通信安全加固”部分。
- 本工具属于开发态工具，不限制安装属主与权限，不限制工具处理文件对象的属主与权限，需用户基于使用场景分配合适的属主、权限，并确保工具处理的文件内容安全可信。

## 运行用户建议

出于安全考虑，在Linux和macOS系统上，**禁止使用**root用户启动MindStudio Insight工具。如果必须使用root用户启动，在确保使用环境安全的前提下，需执行`./MindStudio-Insight --allow-root`进行启动。

## 文件权限控制

在Linux或Unix环境下，通过JupyterLab插件安装使用MindStudio Insight时，请在软件包安装之前，确认该环境中当前用户的umask设置，推荐设置为“0027”或更严格，以确保pip安装后的文件对其他用户和同组用户不可写，避免潜在的安全风险。

**文件权限参考**

| 类型                               | Linux权限参考最大值 |
| ---------------------------------- | ------------------- |
| 用户主目录                         | 750（rwxr-x---）    |
| 程序文件（含脚本文件、库文件等）     | 550（r-xr-x---）    |
| 程序文件目录                       | 550（r-xr-x---）    |
| 配置文件                           | 640（rw-r-----）    |
| 配置文件目录                       | 750（rwxr-x---）    |
| 日志文件（记录完毕或者已经归档）     | 440（r--r-----）    |
| 日志文件（正在记录）                 | 640（rw-r-----）    |
| 日志文件目录                       | 750（rwxr-x---）    |
| Debug文件                          | 640（rw-r-----）    |
| Debug文件目录                      | 750（rwxr-x---）    |
| 临时文件目录                       | 750（rwxr-x---）    |
| 维护升级文件目录                   | 770（rwxrwx---）    |
| 业务数据文件                       | 640（rw-r-----）    |
| 业务数据文件目录                   | 750（rwxr-x---）    |
| 密钥组件、私钥、证书、密文文件目录 | 700（rwx------）    |
| 密钥组件、私钥、证书、加密密文     | 600（rw-------）    |
| 加解密接口、加解密脚本             | 500（r-x------）    |

## 漏洞安全声明

- 华为公司对产品漏洞管理的规定以“漏洞处理流程”为准，详细内容请参见[漏洞处理流程](https://www.huawei.com/cn/psirt/vul-response-process)。
- 如企业客户须获取漏洞信息，请查看[安全通知](https://securitybulletin.huawei.com/enterprise/cn/security-advisory)。
- Windows/macOS/Linux版本安装依赖时，请注意使用满足条件的较新版本软件包，关注并修补存在的漏洞，尤其是已公开的CVSS打分大于7分的高危漏洞。

## 数据安全声明

MindStudio Insight支持导入各类（包括但不限于pytorch memory snapshot、pytorch 
profiler、msprof、memscope、msServiceProfiler等调优工具采集的，详情参考[overview](./overview.md)）调优数据，在导入前请自行确保数据来源可信及环境安全。

## 运行安全声明

MindStudio Insight 在运行异常时会退出进程并打印报错信息，建议根据报错提示定位具体错误原因，错误信息覆盖文件权限，文件解析，数据落盘，数据查询等方面。

## 通信安全加固

- MindStudio Insight通过JupyterLab插件安装使用时，默认为安全使用模式，此时JupyterLab服务启动与网络浏览器在同一台机器上，即在当前机器的Linux系统上使用本地浏览器查看，为安全的localhost内通信。
- MindStudio Insight通过JupyterLab插件安装使用时，如果需要远程查看，为非安全使用模式，此时JupyterLab服务启动与网络浏览器不在同一台机器上，用户需通过修改JupyterLab的配置文件或者通过安装jupyter_server_proxy插件代理端口，以供远程机器访问，为不安全的跨机通信，请自行关注远程通信带来的安全风险。
- 为了消减安全风险，可以通过在JupyterLab插件所在的客户端配置iptables等访问控制策略，或者使用nginx等反向代理工具来加固HTTPS的安全性。此外，针对JupyterLab本身的浏览器防护机制可能存在的风险，可以通过修改JupyterLab的配置文件来进行安全加固。

## 通信矩阵

**通信矩阵信息**

| 序号 | 源设备 | 源IP | 源端口 | 目的设备 | 目的IP | 目的端口（侦听） | 协议 | 端口说明 | 侦听端口是否可更改 | 认证方式 | 加密方式 | 所属平面 | 版本 | 特殊场景 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 安装MindStudio Insight的PC | 安装MindStudio Insight的PC的客户端IP地址 | 系统分配随机端口 | <https://support.huawei.com/ecolumnsweb/en/warranty-policy> 服务器| <https://support.huawei.com/ecolumnsweb/en/warranty-policy> | 443 | HTTPS | 【作用】EULA 说明中的网址【说明】安装时可点击跳转到对应地址。Insight安装时，向用户展示EULA license，阅览时可通过点击该链接自动打开浏览器页签跳转至华为官方产品生命周期终止政策。 | 不涉及 | 不涉及 | SSL | 无 | >=7.0.RC1 | 无 | 无 |
| 2 | 安装MindStudio Insight的PC | 安装MindStudio Insight的PC的客户端IP地址 | 系统分配随机端口 | <https://www.huawei.com/en/psirt/vul-response-process> 服务器 | <https://www.huawei.com/en/psirt/vul-response-process> | 443 | HTTPS | 【作用】EULA 说明中的网址【说明】安装时可点击跳转到对应地址。Insight安装时，向用户展示EULA license，阅览时可通过点击该链接自动打开浏览器页签跳转至华为官方华为漏洞管理原则说明。 | 不涉及 | 不涉及 | SSL | 无 | >=7.0.RC1 | 无 | 无 |
| 3 | 安装MindStudio Insight的PC | 安装MindStudio Insight的PC的客户端IP地址 | 系统分配随机端口 | <https://support.huawei.com/additionalres/pki> 服务器 | <https://support.huawei.com/additionalres/pki> | 443 | HTTPS | 【作用】EULA说明中的网址Insight安装时，向用户展示EULA license，阅览时可通过点击该链接自动打开浏览器页签跳转至华为官方根证书及子证书等下载链接。 | 不涉及 | 不涉及 | SSL | 无 | >=7.0.RC1 | 无 | 无 |
| 4 | 安装MindStudio Insight的PC | 127.0.0.1 | 系统分配随机端口 | 本机 | 127.0.0.1 | 9000~9100 | TCP | MindStudio Insight后端服务开启端口用于跟前端通信，在起始端口开始的100个端口中选一个空闲的| 不涉及 | 不涉及 | 无 | 无 | >=7.0.RC1 | 无 | 无 |
