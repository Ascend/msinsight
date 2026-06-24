# 开发环境搭建

本文说明 MindStudio Insight 开发环境的准备方式，覆盖 Linux、Windows 和 macOS 三个平台。MindStudio Insight 是跨端工具，[开发指南](./develop_guide.md)默认以 Linux 为主线说明快速开发流程；Windows 和 macOS 开发者可参考本文对应平台章节完成环境准备。

> 说明：本文面向源码开发和本地调试，不等同于用户安装指南。用户侧安装请参见[MindStudio Insight 安装指南](../install_guide/mindstudio_insight_install_guide.md)。

## 1. 通用准备

### 1.1 获取代码

建议先 Fork 代码到个人仓库，再 clone 到本地，并配置官方仓库为 upstream。

```bash
git clone https://gitcode.com/<your-user>/msinsight.git
cd msinsight
git remote add upstream https://gitcode.com/Ascend/msinsight.git
git remote -v
```

如只需只读查看源码，也可以直接 clone 官方仓库：

```bash
git clone https://gitcode.com/Ascend/msinsight.git
cd msinsight
```

### 1.2 初始化后端第三方依赖

首次进行后端编译或重新生成 CMake 构建目录前，需下载并预处理第三方依赖。

Linux/macOS：

```bash
cd server/build
python3 download_third_party.py
python3 preprocess_third_party.py
```

Windows：

```powershell
cd server\build
python download_third_party.py
python preprocess_third_party.py
```

> 说明：执行该步骤前请保证网络畅通。若处于代理环境，请提前配置 git、pip、npm/pnpm 等工具的代理或镜像源。

### 1.3 安装前端依赖

```bash
npm install -g pnpm
cd modules
pnpm install
```

### 1.4 配置 pre-commit 代码检查工具

pre-commit 是基于 Git 钩子的代码质量管控工具。项目要求本地启用 pre-commit，提交前完成代码校验和格式规范化。

安装 pre-commit：

```bash
python3 -m pip install pre-commit
```

Windows 如 `python3` 不可用，可使用：

```powershell
python -m pip install pre-commit
```

在项目根目录注册 Git 钩子：

```bash
pre-commit install
```

提交前检查已暂存文件：

```bash
git add <修改过的文件>
pre-commit run
```

如需检查全仓文件：

```bash
pre-commit run --all-files
```

检查过程中，格式化类问题（如代码缩进、换行等）会被自动修复，修复后需重新 `git add <修改过的文件>`。未能自动修复的错误请根据提示人工修复。

前端 `modules` 目录下暂存的 `js/jsx/ts/tsx` 文件会在 pre-commit 阶段执行 ESLint 检查。pre-commit 只检查暂存文件，不能替代 CI 中的全量 `cd modules && pnpm lint`。

## 2. Linux 开发环境

Linux 是开发指南默认采用的本地开发环境。

### 2.1 环境依赖

| 软件名 | 版本要求 | 用途 |
| --- | --- | --- |
| git | 无特殊要求 | 代码拉取与提交 |
| Node.js | v18.20.8+ | 前端开发与构建 |
| pnpm | 建议使用与 lockfile 兼容的版本 | 前端包管理 |
| Python | 3.11+ | 工具脚本、pre-commit、第三方依赖预处理 |
| CMake | 3.16~3.20 | 后端项目构建与编译 |
| GCC/G++ 或 Clang | 使用操作系统稳定版本 | 后端编译 |
| Ninja | 无特殊要求 | 后端构建 |

### 2.2 依赖安装示例

Ubuntu / Debian 系统可参考：

```bash
sudo apt update
sudo apt install -y git python3 python3-pip cmake ninja-build build-essential
```

openEuler / CentOS / RHEL 类系统可参考：

```bash
sudo yum install -y git python3 python3-pip cmake ninja-build gcc gcc-c++
```

Node.js 可通过官方安装包、系统包管理器或版本管理工具安装，并确保版本满足 v18.20.8+。

### 2.3 环境验证

```bash
git --version
node --version
pnpm --version
python3 --version
cmake --version
g++ --version
ninja --version
```

完成环境准备后，可回到[开发指南](./develop_guide.md)继续执行 Linux 快速构建与运行步骤。

## 3. Windows 开发环境

Windows 平台后端编译使用 MinGW 工具链。

### 3.1 基础开发依赖

| 软件名 | 版本要求 | 用途 |
| --- | --- | --- |
| git | 无特殊要求 | 代码拉取与提交 |
| Node.js | v18.20.8+ | 前端开发与构建 |
| pnpm | 建议使用与 lockfile 兼容的版本 | 前端包管理 |
| Python | 3.11+ | 工具脚本、pre-commit、第三方依赖预处理 |
| MinGW | 10.0+（msvcrt 版本）；出包建议 11.0+ | 后端编译 |
| CMake | 3.16~3.20 | 后端项目构建与编译 |

### 3.2 后端工具链配置

后端编译前需保证 MinGW 的 `bin` 目录已加入系统 PATH，且 CMake 能够使用 MinGW 编译器。首次配置完成后，先完成[初始化后端第三方依赖](#12-初始化后端第三方依赖)，再重新生成 CMake 构建目录。

### 3.3 环境验证

```powershell
git --version
node --version
pnpm --version
python --version
cmake --version
g++ --version
```

### 3.4 出包环境入口

如果只进行源码开发和本地调试，完成上述基础依赖即可。如果需要构建 Windows 安装包，还需准备 Rust、Windows 运行时、Ninja、NSIS 和集成 Python 解释器，详见[本地出包环境](#5-本地出包环境)。

## 4. macOS 开发环境

macOS 平台后端编译使用 Clang 工具链。

### 4.1 基础开发依赖

| 软件名 | 版本要求 | 用途 |
| --- | --- | --- |
| git | 无特殊要求 | 代码拉取与提交 |
| Node.js | v18.20.8+ | 前端开发与构建 |
| pnpm | 建议使用与 lockfile 兼容的版本 | 前端包管理 |
| Python | 3.11+ | 工具脚本、pre-commit、第三方依赖预处理 |
| Clang | 15 | 后端编译 |
| CMake | 3.16~3.20 | 后端项目构建与编译 |
| Ninja | 无特殊要求 | 后端构建 |

可通过 Xcode Command Line Tools 安装基础编译工具：

```bash
xcode-select --install
```

Node.js、pnpm、Python、CMake、Ninja 可通过官方安装包或包管理器安装。

### 4.2 环境验证

```bash
git --version
node --version
pnpm --version
python3 --version
cmake --version
clang --version
ninja --version
```

### 4.3 出包环境入口

如果只进行源码开发和本地调试，完成上述基础依赖即可。如果需要构建 macOS 安装包，还需准备 Rust、cargo-bundle、dmgbuild 和集成 Python 解释器，详见[本地出包环境](#5-本地出包环境)。

## 5. 本地出包环境

本地出包会同时构建前端、后端和桌面端底座。Linux 出包复用基础开发依赖；Windows 和 macOS 出包还需要额外准备平台运行时、打包工具和集成 Python 解释器。

### 5.1 Linux 出包

完成 Linux 基础依赖、后端第三方依赖和前端依赖初始化后，可在项目根目录执行：

```bash
cd build
python3 build.py
```

产物位于项目根目录 `out` 目录下。

### 5.2 Windows 出包

#### 5.2.1 环境依赖

| 软件名称 | 版本 | 用途 |
| --- | --- | --- |
| rust | 1.89 | 底座编译构建 |
| Windows 11 SDK | 10.0.22000.0+ | Windows 平台基础开发运行时 |
| MSVC | v143 | Windows 平台基础开发运行时 |
| MinGW | 10.0+（msvcrt 版本）；建议 11.0+ | 后端编译器 |
| Ninja | 无要求 | 后端编译 |
| CMake | 3.16~3.20 | 后端构建 |
| NSIS | 无要求 | 安装包打包软件 |
| nsProcess 插件 | unicode support | 检查是否有重复运行 |
| Node.js | v18.20.8+ | 前端构建 |
| pnpm | 无要求 | 前端构建 |
| Python | 3.11+ | 集群工具打包 |

Python 运行时依赖：

```text
click
tabulate
networkx
jinja2
PyYAML
tqdm
prettytable
ijson
xlsxwriter>=3.0.6
sqlalchemy
numpy<=1.26.4
pandas<=2.3.2
psutil
```

Python 开发时依赖：

```shell
pyinstaller
```

#### 5.2.2 出包步骤

1. 进入项目根目录下 `server/build` 目录，执行：

   ```powershell
   python download_third_party.py
   python preprocess_third_party.py
   ```

2. MindStudio Insight 会在 Windows 出包产物中集成 Python 解释器。请在构建环境上手动安装 Python 解释器（同时包含 pip），建议 Python 版本 3.12.10，并设置环境变量 `MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER` 为 Python 解释器安装目录。该目录需包含 `python.exe`，例如 `D:\xxx\python`。
3. 进入项目根目录下 `build` 目录，执行：

   ```powershell
   python build.py
   ```

产物位于项目根目录 `out` 目录下。

#### 5.2.3 依赖安装附录

- Windows 运行时安装（Windows 11 SDK 和 MSVC）：下载 Visual Studio Installer，双击打开，选择如下依赖（通常默认即可）：

  ![MSVC_install](./figures/MSVC_install.png)

- MinGW 安装：从 [WinLibs](https://www.winlibs.com/) 下载，版本选择 11.0 以上。下载后解压，将解压后 MinGW 路径下的 `bin` 目录添加到系统 PATH 环境变量：

  ![mingw_path_add](./figures/mingw_path_add.png)

  验证安装：终端执行 `g++ -v`，正常输出版本信息即可。

- nsProcess 插件安装：首先安装 NSIS（需装在 `C:\Program Files (x86)` 下）。从 [NsProcess plugin](https://nsis.sourceforge.io/NsProcess_plugin) 获取压缩包，将 `Include/nsProcess.h` 放到 `C:\Program Files (x86)\NSIS\Include`，将 `Plugin/nsProcess.dll` 和 `Plugin/nsProcessw.dll` 放到 `C:\Program Files (x86)\NSIS\Plugins\x86-unicode`。
- Rust：可通过 [rustup](https://www.rust-lang.org) 安装，验证：`rustc --version` 和 `cargo --version`。
- Ninja：通过 [官网](https://ninja-build.org) 下载二进制文件或包管理器安装，验证：`ninja --version`。
- Node.js：通过 [官网](https://nodejs.org) 安装 LTS 版本（v18.20.8+），验证：`node --version`。
- pnpm：执行 `npm install -g pnpm`，验证：`pnpm --version`。
- Python：通过 [官网](https://www.python.org) 安装 3.11+，勾选“Add Python to PATH”，验证：`python --version`。

### 5.3 macOS 出包

#### 5.3.1 环境依赖

| 软件名称 | 版本 | 用途 |
| --- | --- | --- |
| rust | 1.89 | 底座编译构建 |
| cargo-bundle | 无要求 | 打包 |
| Ninja | 无要求 | 后端编译 |
| Node.js | v18.20.8+ | 前端构建 |
| pnpm | 无要求 | 前端构建 |
| Python | 3.11+ | 集群工具打包 |
| Clang | 15 | 编译 |
| CMake | 3.16~3.20 | 后端构建 |
| dmgbuild | 无要求 | dmg 产物构建 |

Python 运行时依赖：

```text
click
tabulate
networkx
jinja2
PyYAML
tqdm
prettytable
ijson
xlsxwriter>=3.0.6
sqlalchemy
numpy<=1.26.4
pandas<=2.3.2
psutil
dmgbuild
```

Python 开发时依赖：

```shell
pyinstaller
```

#### 5.3.2 出包步骤

**Step 1. 预处理构建依赖**

```bash
cd server/build
python3 download_third_party.py
python3 preprocess_third_party.py
```

**Step 2. 指定 APP 签名证书（可选）**

> 注意：请您确保已阅读并知悉 [LICENSE](https://gitcode.com/Ascend/msinsight/blob/master/docs/LICENSE) 要求。

Insight macOS ARM 版本在构建出包时，会对产物 APP 进行 macOS 开发者证书签名。您可以通过环境变量配置签名证书。如不指定，缺省时使用临时证书签名，可能导致产物无法通过网络分发（本地调试运行不受影响）。

- 证书使用前置：要求为可用于签名的苹果开发者证书，并确保已正确导入钥匙串中（如登录钥匙串 `~/Library/Keychains/login.keychain`）。
- 通过环境变量配置证书，支持证书名或证书 ID。

```bash
export INSIGHT_APP_SIGN="insight_cert"
security unlock-keychain -p {您当前用户的密码} ~/Library/Keychains/login.keychain
```

**Step 3. 设置集成 Python 解释器的环境变量**

MindStudio Insight 会在 macOS 出包产物中集成 Python 解释器：

- 第一步：在构建环境上手动安装可移植的 Python 解释器（同时包含 pip），建议 Python 版本 3.12.10。
- 第二步：设置环境变量 `MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER` 为 Python 解释器安装目录。该目录需包含 `bin/python3`，例如 `/Users/xxx/python`。如 Python 版本不为 3.12，需手动修改 `server/build/build.py` 中的 version 变量值。

“可移植”指将 A 机器上的 Python 文件夹拷贝到 B 机器上仍可直接使用。macOS 上某些 Python 版本依赖 `/Library` 下的动态库，需确保安装的是可移植版本。

**Step 4. 执行出包脚本**

进入项目根目录下 `build` 目录，执行：

```bash
python3 build.py
```

产物位于项目根目录 `out` 目录下。
