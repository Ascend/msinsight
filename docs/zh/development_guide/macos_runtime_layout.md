# macOS 资源布局与解析运行时校验

## 安装包约定

macOS 桌面包以真实的 `Contents/Resources` 为资源根目录：

```text
MindStudioInsight.app/Contents/
├── MacOS/MindStudioInsight
└── Resources/
    ├── python/bin/python3
    └── profiler/
        ├── frontend/
        └── server/
            ├── profiler_server
            └── insight_web_agent/
```

Rust 底座根据自身安装位置解析资源根，直接启动该目录中的后端和 ACP，
并通过 `--resourceRoot=<绝对路径>` 将资源根传给后端。
ACP 的 `--resource-path` 指向实际的 `insight_web_agent` 目录。
WebView 的逻辑 URL 仍使用 `/resources/profiler/frontend/`，其磁盘位置为标准资源目录。

后端的 macOS 发布构建使用 `<resourceRoot>/python/bin/python3`。
直接启动后端而未提供资源根时，只识别 `Contents/Resources/profiler/server` 标准结构。
应用层不再依赖 `MacOS/resources` 或 App 根目录下的资源桥接链接。

## 最终 App 的构建门禁

Python 执行权限恢复和签名结束后，打包过程依次执行：

1. 检查标准位置的 Python 文件及执行权限。
2. 启动包内后端的 `--checkPython` 模式，通过生产 `PythonUtil` 路径解析与进程启动代码运行内置 Python。
3. 检查隔离模式下的 `ctypes`、`pickle`、`sqlite3`、`ssl` 导入及退出状态；`-B` 禁止在签名后的资源树内写入字节码缓存。
4. 执行 RAG 冒烟；对签名的 arm64 包再次严格验证最终签名。

运行时探针的工作目录是包外临时目录，后端日志不会写进签名资源树。
探针失败或超时会阻止生成 DMG。

人工验证时应从包外目录运行，例如：

```bash
APP="/Applications/MindStudioInsight.app"
DYLD_LIBRARY_PATH="$APP/Contents/Resources/profiler/server${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
  "$APP/Contents/Resources/profiler/server/profiler_server" \
  --resourceRoot="$APP/Contents/Resources" --checkPython
```

完整发布验收还应在 arm64、x86_64 上分别安装最终 DMG，覆盖含空格路径、Finder 启动、
无解析缓存的快照导入，以及 ACP/pt_snap。构建期探针不能替代这些功能验收。

## 非 macOS 路径兼容性与 Windows 验收

`runtime_paths::resources_root` 仅在 macOS 执行 `canonicalize`，继续解析可执行目录和
`Contents/Resources` 的真实位置。Windows/Linux 保持既有的 `<exe_dir>/resources`
路径拼接，不改写路径表示、不解析软链，也不在此处新增目录存在性校验。

Windows 的 `std::fs::canonicalize` 会产生 `\\?\` 扩展长度路径。部分受支持的 Node.js
版本在 CLI 入口的 realpath 阶段不能处理这种表示，即使入口后缀为 `.mjs`，也可能在 ESM
加载之前以 `EISDIR` 退出。参见 [Node.js 上游问题 #62446](https://github.com/nodejs/node/issues/62446)。
本修复只避免新增该前缀，不承诺把原本就是 verbatim 或必须使用扩展长度语法的路径转换为普通路径。

回归测试覆盖非 macOS 的原始路径表示（含中文、空格、`..` 及不存在的目录）、Linux 软链别名，
以及 macOS 可执行目录和资源目录软链的物理解析。常规测试与构建在 CI/验证机执行。

### Windows 实际 Node CLI 回归

在隔离的 Windows CI/验证机上使用普通、较短的临时目录。分别使用用户发生故障时的实际 Node
可执行文件/版本，以及已知受影响版本（例如 25.1.0）执行下面的显式测试；另外覆盖上游修复版。
不要仅用已修复的新 Node 通过来证明回归被覆盖。Node 版本号与 Insight 产品版本号分别记录。

```powershell
$env:INSIGHT_TEST_NODE = 'C:\validation\node-v25.1.0-win-x64\node.exe'
& $env:INSIGHT_TEST_NODE -p 'JSON.stringify({version:process.version,execPath:process.execPath})'
cargo test --manifest-path platform/Cargo.toml --no-default-features `
  runtime_paths::tests::windows_node_cli_accepts_resource_entry_path -- --ignored --exact
```

该测试默认忽略，必须由上述独立验证步骤显式运行并设置超时，不能把默认测试通过视为已验收。
它在含中文/空格的安装目录形状中创建 `.mjs` 入口，调用生产资源路径解析，再通过
`Command::new(node).arg(entry)` 真正启动 Node，要求退出码为 0 且收到预期输出。
单独测试路径字符串、`pathToFileURL` 或 Linux 上的 Node 不替代此步骤。

### 最终安装包 ACP ready 验收

1. 记录待验收提交 SHA、最终 Windows 产物与构建记录的对应关系；在隔离验证机安装，不能沿用旧包。
2. 覆盖普通目录和含中文/空格的目录。由桌面程序实际使用的 `PATH` 确认 Node 可执行文件和版本，
   不仅记录另一个终端的 `node --version`。不要把 capability token 写入日志或截图。
3. 启动桌面程序并打开 Agent，确认 Node 子进程实际入口为安装目录下的
   `resources/profiler/server/insight_web_agent/index.mjs`，普通路径未被本函数改写为 verbatim 路径。
4. 确认 ACP 子进程持续存活、本地端口就绪，前端状态最终为 `ready` 而不是停留在 `starting`
   或进入 `start-failed`；再验证一次本地 Agent 请求完成，不能只以端口可连接作为功能通过。
5. 保存去敏后的命令路径、Node 版本、退出状态/错误、界面结果和所用产物 SHA。失败时保留 stderr，
   不把“已找到 Node，但服务未能启动”的通用提示直接等同于已证实的 `EISDIR`。

路径单测和最小 `.mjs` CLI 测试均不能替代最终包的 ACP ready 与请求验收。

## 解析失败终态

Framework 在发起导入请求前登记待导入快照的来源路径，将轮次、来源与失败终态一起传给模块。
目录内容在响应前可能未知，路径归属必须能接住目录内快照的快速失败，不能只依赖旧的活动数据源。
只有识别为快照的导入响应使用快照轮次过期保护；普通导入的错误处理、模块通知与项目更新必须保留。
目录最终识别为普通数据后，其模块通知不携带启动时的快照轮次，避免下游误拒收并残留失败缓存。

快照 `parse/fail` 只有匹配已登记来源或已确认的当前快照时才可以清理页面。
当前文件 ID 为空不代表可以接受任意快照的失败。缺失、空白的错误统一显示非空默认信息。
轮次、来源与失败状态在初始化时原子回放，iframe 晚于失败事件就绪也能恢复失败终态。
缓存的失败与当前展示分开：冷启动 iframe 接收 A 的失败缓存、B 的当前状态时保留 A 的缓存，
不清理或报错干扰 B；只有之后选回 A 时才恢复 A 的失败终态。
如果当前 B 已有失败终态，其他文件的迟到失败缓存不能替换它，也不能让 B 的进度或初始化重新开启加载。

同轮延迟的导入通知、iframe 加载通知和进度不能重新开启失败任务的加载状态。
初始化消息中同轮的空失败字段不能撤销已有失败终态；清除应由新导入轮次、明确的普通导入响应或重置操作触发。
从失败快照 A 切到成功快照 B，再选回 A 时，应清理 B 的查询与显示状态并恢复 A 的失败，
不能仅关闭 loading 后保留 B 的内容。显式重新导入开启新轮次，允许同一文件重试。
前端轮次不替代后端任务取消或服务端任务标识，也不能辨别同文件重试时旧后端任务的无标识通知。

## 整合补丁验收清单

以下检查交由 CI 和对应平台验证机执行。新增测试或静态补丁检查不等同于已运行通过。

- Framework：空会话及旧数据源下，快照失败早于导入响应；目录内快照快速失败；普通 JSON 和目录并发导入的两个响应均到达模块，并清理应清除的旧失败缓存。
- Leaks：无关迟到失败不清空当前数据；失败后延迟初始化不复活加载；同文件显式重试；A 失败、B 成功后选回 A（包含全新 iframe 的冷初始化）；空错误兜底。
- 前端初始化：轮次、来源与失败终态一致，旧活动目录通知不能覆盖正在导入的新目标。
- Rust：Windows/Linux 保留路径表示，macOS 保留物理路径解析；HTTP 资源缺失和 Python 脚本失败输出诊断并非零退出，不 panic。
- 后端：macOS Debug/Release 分开验证解释器选择；缺失、非目录资源根和非标准布局均有可捕获诊断；探针成功和失败均不创建服务日志。
- 测试隔离：参数单例在断言失败或异常退出时恢复原值；POSIX 真实脚本测试在 Windows 跳过，跨平台 mock 测试仍验证探针调用约定。
- 安装包：Windows 实际 Node CLI 与 ACP ready 验收，以及 macOS 双架构最终 DMG 的 Finder 启动、含空格路径和真实快照导入，按上文执行。

相关开发与测试入口见[开发指南](./develop_guide.md)。
