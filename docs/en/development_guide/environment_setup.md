# Development Environment Setup

This document describes how to prepare the MindStudio Insight development environment, covering Linux, Windows, and macOS platforms. MindStudio Insight is a cross-platform tool, and the [Development Guide](./develop_guide.md) uses Linux as the primary platform by default to illustrate the rapid development workflow. Windows and macOS developers can refer to the corresponding platform sections in this document to complete environment preparation.

> **NOTE** This document is intended for source code development and local debugging, and is not equivalent to the user installation guide. For user-side installation, see [MindStudio Insight Installation Guide](../install_guide/mindstudio_insight_install_guide.md).

## 1. General Preparation

### 1.1 Code Pull

It is recommended to fork the code to your personal repository first, then clone it locally, and configure the official repository as upstream.

```bash
git clone https://gitcode.com/<your-user>/msinsight.git
cd msinsight
git remote add upstream https://gitcode.com/Ascend/msinsight.git
git remote -v
```

If you only need read-only access to the source code, you can also clone the official repository directly:

```bash
git clone https://gitcode.com/Ascend/msinsight.git
cd msinsight
```

### 1.2 Initializing Backend Third-party Dependencies

Before performing backend compilation for the first time or regenerating the CMake build directory, you need to download and preprocess third-party dependencies.

Linux/macOS:

```bash
cd server/build
python3 download_third_party.py
python3 preprocess_third_party.py
```

Windows:

```powershell
cd server\build
python download_third_party.py
python preprocess_third_party.py
```

> **NOTE** Ensure network connectivity before executing this step. If you are in a proxy environment, configure the proxy or mirror source for tools such as git, pip, and npm/pnpm in advance.

### 1.3 Installing Frontend Dependencies

```bash
npm install -g pnpm
cd modules
pnpm install
```

### 1.4 Configuring the pre-commit Code Checking Tool

pre-commit is a code quality control tool based on Git hooks. The project requires pre-commit to be enabled locally to complete code verification and format normalization before committing.

Install pre-commit.

```bash
python3 -m pip install pre-commit
```

On Windows, if <code>python3</code> is unavailable, you can use:

```powershell
python -m pip install pre-commit
```

Register Git hooks in the project root directory.

```bash
pre-commit install
```

Check staged files before commit.

```bash
git add <Modified files>
pre-commit run
```

To check all files in the repository:

```bash
pre-commit run --all-files
```

During the check, formatting issues (such as code indentation and line breaks) are fixed automatically. After the fix, run `git add <modified files>` again. For errors that cannot be fixed automatically, fix them manually as prompted.

Staged `js/jsx/ts/tsx` files under the frontend `modules` directory are checked by ESLint during the pre-commit phase. pre-commit only checks staged files and cannot replace the full `cd modules && pnpm lint` check in CI.

## 2. Linux Development Environment

Linux is the default local development environment used in this development guide.

### 2.1 Environment Dependencies

| Software Name | Version Requirements | Purpose |
| --- | --- | --- |
| git | No special requirements | Code Pull and Commit |
| Node.js | v18.20.8+ | Frontend development and build |
| pnpm | It is recommended to use a version compatible with the lockfile. | Frontend package management |
| Python | 3.11+ | Tool Scripts, pre-commit, third-party dependency preprocessing |
| CMake | 3.16~3.20 | Backend project build and compilation |
| GCC/G++ or Clang | Use the stable version of the operating system. | Backend compilation |
| Ninja | No special requirements | Backend build |

### 2.2 Dependency Installation Example

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y git python3 python3-pip cmake ninja-build build-essential
```

openEuler/CentOS/RHEL:

```bash
sudo yum install -y git python3 python3-pip cmake ninja-build gcc gcc-c++
```

`Node.js` can be installed via the official installation package, system package manager, or version management tool. Ensure that the version v18.20.8+ is used.

### 2.3 Environment Verification

```bash
git --version
node --version
pnpm --version
python3 --version
cmake --version
g++ --version
ninja --version
```

### 2.4 Recommended VSCode Configuration

On Linux, VSCode can be used as the main development entry. It is recommended to install the following extensions.

| Extension | Purpose |
| --- | --- |
| C/C++ | Completion, navigation, and debugging for backend C/C++ code |
| CMake Tools | CMake configuration, build, and target management for the backend |
| ESLint | Frontend code checking |
| Prettier | Frontend code formatting (used according to team specifications) |
| Python | Editing of Python build scripts and utility scripts |

After opening the repository for the first time, it is recommended to confirm that the VSCode integrated terminal uses the default Linux shell, and complete [Initializing Backend Third-party Dependencies](#12-initializing-backend-third-party-dependencies) and [Installing Frontend Dependencies](#13-installing-frontend-dependencies) in the terminal.

### 2.5 VSCode Frontend and Backend Compilation, Startup, and Debugging Configuration

The project `.gitignore` ignores the `.vscode` directory by default, so it is not recommended to commit workspace-level VSCode configurations directly. You can create `.vscode/tasks.json` and `.vscode/launch.json` locally as the entry for frontend and backend compilation, startup, and debugging on Linux + VSCode.

#### 2.5.1 tasks.json

`.vscode/tasks.json` is mainly used to run initialization, compilation, and frontend and backend development service startup tasks:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "MSInsight: init backend third-party",
      "type": "shell",
      "command": "python3 download_third_party.py && python3 preprocess_third_party.py",
      "options": {
        "cwd": "${workspaceFolder}/server/build"
      },
      "problemMatcher": []
    },
    {
      "label": "MSInsight: install frontend deps",
      "type": "shell",
      "command": "pnpm install",
      "options": {
        "cwd": "${workspaceFolder}/modules"
      },
      "problemMatcher": []
    },
    {
      "label": "MSInsight: build backend",
      "type": "shell",
      "command": "python3 build.py build",
      "options": {
        "cwd": "${workspaceFolder}/server/build"
      },
      "dependsOn": [
        "MSInsight: init backend third-party"
      ],
      "problemMatcher": "$gcc"
    },
    {
      "label": "MSInsight: build frontend",
      "type": "shell",
      "command": "pnpm build",
      "options": {
        "cwd": "${workspaceFolder}/modules"
      },
      "dependsOn": [
        "MSInsight: install frontend deps"
      ],
      "problemMatcher": []
    },
    {
      "label": "MSInsight: build backend + frontend",
      "dependsOrder": "sequence",
      "dependsOn": [
        "MSInsight: build backend",
        "MSInsight: build frontend"
      ],
      "problemMatcher": []
    },
    {
      "label": "MSInsight: run backend ws 9000",
      "type": "shell",
      "command": "bash -c 'ws_port=9000; ./profiler_server --wsPort=$ws_port --logPath=\"${workspaceFolder}/.vscode/msinsight-log\" --notStrict & server_pid=$!; trap \"kill $server_pid 2>/dev/null\" EXIT; until (echo > /dev/tcp/127.0.0.1/$ws_port) >/dev/null 2>&1; do if ! kill -0 $server_pid 2>/dev/null; then wait $server_pid; exit $?; fi; sleep 1; done; echo \"MSInsight backend ready on $ws_port\"; wait $server_pid'",
      "options": {
        "cwd": "${workspaceFolder}/server/output/linux-${input:linuxArch}/bin"
      },
      "isBackground": true,
      "problemMatcher": {
        "owner": "msinsight-backend",
        "pattern": {
          "regexp": "."
        },
        "background": {
          "activeOnStart": true,
          "beginsPattern": ".*",
          "endsPattern": "MSInsight backend ready on 9000"
        }
      }
    },
    {
      "label": "MSInsight: run frontend framework",
      "type": "shell",
      "command": "pnpm start",
      "options": {
        "cwd": "${workspaceFolder}/modules/framework"
      },
      "isBackground": true,
      "problemMatcher": {
        "owner": "msinsight-frontend",
        "pattern": {
          "regexp": "."
        },
        "background": {
          "activeOnStart": true,
          "beginsPattern": ".*",
          "endsPattern": "(Compiled successfully|webpack compiled successfully|Local:|ready in)"
        }
      }
    },
    {
      "label": "MSInsight: run browser dev",
      "dependsOrder": "parallel",
      "dependsOn": [
        "MSInsight: run backend ws 9000",
        "MSInsight: run frontend framework"
      ],
      "problemMatcher": []
    },
    {
      "label": "MSInsight: cargo run platform dev",
      "type": "shell",
      "command": "cargo run --features dev",
      "options": {
        "cwd": "${workspaceFolder}/platform"
      },
      "dependsOn": [
        "MSInsight: run browser dev"
      ],
      "problemMatcher": []
    },
    {
      "label": "MSInsight: package Linux release",
      "type": "shell",
      "command": "python3 build.py",
      "options": {
        "cwd": "${workspaceFolder}/build"
      },
      "problemMatcher": []
    }
  ],
  "inputs": [
    {
      "id": "linuxArch",
      "type": "pickString",
      "description": "Select the architecture directory in server/output/linux-<arch>/bin",
      "options": [
        "x86_64",
        "aarch64"
      ],
      "default": "x86_64"
    }
  ]
}
```

Common task descriptions

| Task | Purpose |
| --- | --- |
| `MSInsight: build backend` | Initializes backend third-party dependencies and compiles `profiler_server` |
| `MSInsight: build frontend` | Installs frontend dependencies and builds all frontend modules |
| `MSInsight: build backend + frontend` | Runs the backend and frontend builds in sequence |
| `MSInsight: run browser dev` | Starts the backend WebSocket service and the frontend `framework` development service in parallel, suitable for browser debugging |
| `MSInsight: cargo run platform dev` | Starts the Rust base development mode window. The window loads `http://localhost:5174?port=9000` |
| `MSInsight: package Linux release` | Performs local Linux packaging. The output is placed in the `out` directory at the project root |

After running `MSInsight: run browser dev`, open `http://localhost:5174` in a browser to access the development environment. If backend port `9000` is occupied, you can copy the task and change the port number in `ws_port` and `problemMatcher.background.endsPattern`, and also change the `port` parameter in the frontend access address. Alternatively, close the running Insight desktop application first. Running `MSInsight: cargo run platform dev` starts the desktop development mode window; this mode corresponds to `platform/src/dev.rs` and connects to frontend port `5174` and backend WebSocket port `9000` by default.

#### 2.5.2 launch.json

`.vscode/launch.json` is used to configure frontend and backend debugging commands. Backend debugging uses the VSCode C/C++ extension to start `profiler_server` through `gdb`, and frontend debugging uses the Chrome DevTools Protocol to connect to the `framework` development service.

> Note: If you only need to debug the frontend pages, use `MSInsight: Frontend Debug`. If you need to debug both the frontend and the backend, use `MSInsight: Full Stack Debug`. This combination starts the backend `profiler_server` debugging process and, after starting the frontend development service, opens Chrome.

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "MSInsight: Backend Debug",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/server/output/linux-${input:linuxArch}/bin/profiler_server",
      "args": [
        "--wsPort=9000",
        "--logPath=${workspaceFolder}/.vscode/msinsight-log",
        "--notStrict"
      ],
      "cwd": "${workspaceFolder}/server/output/linux-${input:linuxArch}/bin",
      "stopAtEntry": false,
      "environment": [
        {
          "name": "LD_LIBRARY_PATH",
          "value": "${workspaceFolder}/server/output/linux-${input:linuxArch}/bin:${env:LD_LIBRARY_PATH}"
        }
      ],
      "externalConsole": false,
      "MIMode": "gdb",
      "setupCommands": [
        {
          "description": "Enable pretty-printing for gdb",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        }
      ]
    },
    {
      "name": "MSInsight: Frontend Debug",
      "type": "chrome",
      "request": "launch",
      "url": "http://localhost:5174?port=9000",
      "webRoot": "${workspaceFolder}/modules/framework/src",
      "preLaunchTask": "MSInsight: run frontend framework",
      "sourceMaps": true,
      "sourceMapPathOverrides": {
        "webpack://@insight/framework/src/*": "${workspaceFolder}/modules/framework/src/*",
        "webpack://@insight/*/src/*": "${workspaceFolder}/modules/*/src/*",
        "webpack:///src/*": "${workspaceFolder}/modules/framework/src/*"
      }
    }
  ],
  "compounds": [
    {
      "name": "MSInsight: Full Stack Debug",
      "configurations": [
        "MSInsight: Backend Debug",
        "MSInsight: Frontend Debug"
      ],
      "stopAll": true
    }
  ],
  "inputs": [
    {
      "id": "linuxArch",
      "type": "pickString",
      "description": "Select the architecture directory in server/output/linux-<arch>/bin",
      "options": [
        "x86_64",
        "aarch64"
      ],
      "default": "x86_64"
    }
  ]
}
```

Common debugging configuration descriptions.

| Configuration | Purpose |
| --- | --- |
| `MSInsight: Backend Debug` | Uses `gdb` to start and debug the backend `profiler_server`, with the startup parameters `--wsPort=9000 --logPath=${workspaceFolder}/.vscode/msinsight-log --notStrict` |
| `MSInsight: Frontend Debug` | Starts the `framework` frontend development service and opens Chrome to debug `http://localhost:5174?port=9000` |
| `MSInsight: Full Stack Debug` | Starts the backend debugging process and the frontend Chrome debugging session at the same time |

Notes:

- The backend `cppdbg` configuration depends on the VSCode C/C++ extension and the local `gdb`; if you use `lldb-mi` or another debugger, adjust `MIMode` according to the local toolchain.
- Before debugging the backend for the first time, run `MSInsight: build backend` to ensure that `server/output/linux-<arch>/bin/profiler_server` has been generated.
- The `port=9000` in the frontend entry URL must be consistent with the backend `--wsPort=9000`.
- If port `9000` is occupied, change both the backend `--wsPort` in `launch.json` and the `port` parameter in the frontend URL.
- VSCode background tasks do not end automatically. After debugging, you can manually stop the corresponding `pnpm start` terminal in the VSCode Terminal panel.

After completing the environment preparation, you can return to the [Development Guide](./develop_guide.md) to continue with the Linux quick build and run steps.

## 3. Windows Development Environment

The Windows platform uses the MinGW toolchain for backend compilation.

### 3.1 Basic Development Dependencies

| Software Name | Version Requirements | Purpose |
| --- | --- | --- |
| git | No special requirements | Code Pull and Commit |
| Node.js | v18.20.8+ | Frontend development and build |
| pnpm | It is recommended to use a version compatible with the lockfile. | Frontend package management |
| Python | 3.11+ | Tool Scripts, pre-commit, third-party dependency preprocessing |
| MinGW | 10.0+ (msvcrt version); 11.0+ recommended for packaging | Backend compilation |
| CMake | 3.16~3.20 | Backend project build and compilation |

### 3.2 Backend Toolchain Configuration

Before backend compilation, ensure that the `bin` directory of MinGW has been added to the system PATH and that CMake can use the MinGW compiler. After the initial configuration is complete, first finish [initializing backend third-party dependencies](#12-initializing-backend-third-party-dependencies), and then regenerate the CMake build directory.

### 3.3 Environment Verification

```powershell
git --version
node --version
pnpm --version
python --version
cmake --version
g++ --version
```

### 3.4 Packaging Environment Entry

If you only perform source code development and local debugging, completing the above basic dependencies is sufficient. If you need to build a Windows installation package, you must also prepare Rust, Windows Runtime, Ninja, NSIS, and an integrated Python interpreter. For details, see [Local Packaging Environment](#5-local-packaging-environment).

## 4. macOS Development Environment

On macOS, the backend compilation uses the Clang toolchain.

### 4.1 Basic Development Dependencies

| Software Name | Version Requirements | Purpose |
| --- | --- | --- |
| git | No special requirements | Code Pull and Commit |
| Node.js | v18.20.8+ | Frontend development and build |
| pnpm | It is recommended to use a version compatible with the lockfile. | Frontend package management |
| Python | 3.11+ | Tool Scripts, pre-commit, third-party dependency preprocessing |
| Clang | 15 | Backend compilation |
| CMake | 3.16~3.20 | Backend project build and compilation |
| Ninja | No special requirements | Backend build |

Basic compilation tools can be installed via Xcode command line tools:

```bash
xcode-select --install
```

Node.js, pnpm, Python, CMake, and Ninja can be installed via official installation packages or package managers.

### 4.2 Environment Verification

```bash
git --version
node --version
pnpm --version
python3 --version
cmake --version
clang --version
ninja --version
```

### 4.3 Packaging Environment Entry

If you only need source code development and local debugging, completing the basic dependencies above is sufficient. If you need to build a macOS installation package, you must also prepare Rust, cargo-bundle, dmgbuild, and an integrated Python interpreter. For details, see [Local Packaging](#5-local-packaging-environment).

## 5. Local Packaging Environment

Local packaging builds the frontend, backend, and desktop base simultaneously. Linux packaging reuses basic development dependencies; Windows and macOS packaging also require additional platform runtimes, packaging tools, and an integrated Python interpreter.

### 5.1 Linux Packaging

After completing the initialization of Linux basic dependencies, backend third-party dependencies, and frontend dependencies, execute the following commands under the project root directory:

```bash
cd build
python3 build.py
```

Artifacts are located under the `out` directory of the project root directory.

### 5.2 Windows Packaging

#### 5.2.1 Environment Dependencies

| Software Name | Version | Purpose |
| --- | --- | --- |
| rust | 1.89 | Base Compilation and Build |
| Windows 11 SDK | 10.0.22000.0+ | Basic development runtime for Windows platform |
| MSVC | v143 | Basic development runtime for Windows platform |
| MinGW | 10.0+ (msvcrt version); 11.0+ recommended | Backend Compiler |
| Ninja | No requirement | Backend Compilation |
| CMake | 3.16~3.20 | Backend Build |
| NSIS | No requirement | Installer packaging software |
| nsProcess plugin | unicode support | Check for duplicate running instances |
| Node.js | v18.20.8+ | Frontend Build |
| pnpm | No requirement | Frontend Build |
| Python | 3.11+ | Cluster Tool Packaging |

Python runtime dependencies:

```text
click
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

Python development dependencies:

```shell
pyinstaller
```

#### 5.2.2 Packaging Steps

1. Enter the `server/build` directory under the project root directory and run:

   ```powershell
   python download_third_party.py
   python preprocess_third_party.py
   ```

2. MindStudio Insight integrates a Python interpreter into the Windows packaging artifacts. Manually install a Python interpreter (including pip) on the build environment. Python 3.12.10 is recommended. Set the environment variable `MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER` to the Python interpreter installation directory. This directory must contain `python.exe`, for example, `D:\xxx\python`.
3. Enter the `build` directory under the project root directory and run:

   ```powershell
   python build.py
   ```

Artifacts are located in the `out` directory under the project root directory.

#### 5.2.3 Dependency Installation

- Windows Runtime (Windows 11 SDK and MSVC): Download Visual Studio Installer, double-click to open it, and select the following dependencies (the default selection is usually sufficient).

  ![MSVC_install](./figures/MSVC_install.png)

- MinGW: Download from [WinLibs](https://www.winlibs.com/), and select version 11.0 or later. After downloading, extract the archive, and add the `bin` directory under the extracted MinGW path to the system PATH environment variable.

  ![mingw_path_add](./figures/mingw_path_add.png)

  Verify the installation: run `g++ -v` in the terminal. The version information should be output normally.

- nsProcess plugin: first install NSIS (must be installed under `C:\Program Files (x86)`). Obtain the compressed package from [NsProcess plugin](https://nsis.sourceforge.io/NsProcess_plugin), place `Include/nsProcess.h` into `C:\Program Files (x86)\NSIS\Include`, and place `Plugin/nsProcess.dll` and `Plugin/nsProcessw.dll` into `C:\Program Files (x86)\NSIS\Plugins\x86-unicode`.
- Rust: can be installed via [rustup](https://www.rust-lang.org). Verify with `rustc --version` and `cargo --version`.
- Ninja: download the binary from the [official website](https://ninja-build.org) or install via a package manager. Verify with `ninja --version`.
- Node.js: install the LTS version (v18.20.8+) from the [official website](https://nodejs.org). Verify with `node --version`.
- pnpm: Run `npm install -g pnpm`. Verify with `pnpm --version`.
- Python: Install 3.11+ from the [official website](https://www.python.org). Check "Add Python to PATH". Verify with `python --version`.

### 5.3 macOS Packaging

#### 5.3.1 Environment Dependencies

| Software Name | Version | Purpose |
| --- | --- | --- |
| rust | 1.89 | Base compilation and build |
| cargo-bundle | N/A | Packaging |
| Ninja | N/A | Backend compilation |
| Node.js | v18.20.8+ | Frontend build |
| pnpm | N/A | Frontend build |
| Python | 3.11+ | Cluster tool packaging |
| Clang | 15 | Compilation |
| CMake | 3.16~3.20 | Backend build |
| dmgbuild | N/A | dmg artifact build |

Python runtime dependencies:

```text
click
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

Python development dependencies:

```shell
pyinstaller
```

#### 5.3.2 Packaging Steps

**Step 1. Preprocess build dependencies**.

```bash
cd server/build
python3 download_third_party.py
python3 preprocess_third_party.py
```

**Step 2. (Optional) Specify APP signing certificate.**

> NOTE Ensure that you have read and understood the [LICENSE](https://gitcode.com/Ascend/msinsight/blob/master/docs/LICENSE) requirements.

When building and packaging the Insight macOS ARM version, the resulting APP is signed with a macOS developer certificate. You can configure the signing certificate via an environment variable. If not specified, an ad-hoc certificate is used by default, which may prevent the artifact from being distributed over the network (local debugging and running are not affected).

- Certificate prerequisites: An Apple Developer certificate valid for signing is required, and it must be correctly imported into the keychain (such as the login keychain `~/Library/Keychains/login.keychain`).
- Configure the certificate via environment variables, supporting either the certificate name or certificate ID.

```bash
export INSIGHT_APP_SIGN="insight_cert"
security unlock-keychain -p {You password} ~/Library/Keychains/login.keychain
```

**Step 3: Set the environment variable for the integrated Python interpreter**.

MindStudio Insight integrates a Python interpreter into the macOS packaging artifacts.

- Step 1: Manually install a portable Python interpreter (including pip) on the build environment. Python version 3.12.10 is recommended.
- Step 2: Set the environment variable `MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER` to the Python interpreter installation directory. This directory must contain `bin/python3`, for example `/Users/xxx/python`. If the Python version is not 3.12, manually modify the `version` variable value in `server/build/build.py`.

"Portable" means that the Python folder on machine A can be copied to machine B and used directly. Some Python versions on macOS depend on dynamic libraries under `/Library`, so you must ensure that the installed version is portable.

**Step 4. Execute the packaging script**.

Enter the `build` directory under the project root directory and execute:

```bash
python3 build.py
```

Artifacts are located in the `out` directory under the project root directory.
