# MSInsight MCP 模块说明文档

本文档面向开发者与集成方，系统说明 `mcp/` 模块的整体架构、代码职责、运行方式与扩展指南。

## 1. 模块目标

`mcp/` 的职责是把现有 C++ 后端（WebSocket 服务）桥接成一个可被 AI Agent 调用的 MCP 服务。

- 对内：通过 WebSocket 连接 C++ 后端
- 对外：提供 MCP 协议（支持 `stdio`、`sse`、`websocket` 三种传输）
- 中间：做请求封装、响应反序列化、工具注册、错误转换与日志记录

这是一种典型的"代理桥接模式"：

```shell
AI Agent / LangChain
        |
        | MCP (stdio / SSE / WebSocket)
        v
Python MCP Bridge (mcp/)
        |
        | WebSocket JSON
        v
C++ Profiling Backend
```

## 2. 架构分层

重构后采用四层清晰架构：

```shell
┌─────────────────────────────────────────────────────┐
│  MCP Server Layer (mcp_server.py)                   │
│  Server 实例 + list_tools / call_tool 分发           │
├─────────────────────────────────────────────────────┤
│  Tool Handler Layer (tools/*/)                      │
│  接收 MCP 参数 → 调用 mapping → 返回 CallToolResult  │
│  ├── loader/      导入工具 (1 tool)                  │
│  │   └── global/  全局工具 (1 tool 启用, 7 预留)     │
│  ├── timeline/    时间线工具 (4 tools)               │
│  ├── cluster/     聚类工具 (3 tools)                 │
│  └── operator/    算子/内存/汇总工具 (3 tools 启用,  │
│                    6 预留；直调 cpp_client，绕过映射层)│
├─────────────────────────────────────────────────────┤
│  Mapping Layer (mapping/)                           │
│  C++ 后端 API 的唯一封装处，所有 handler 不直接引用  │
│  cpp_client，而是调用本层函数                        │
│  注：loader/global 与 operator 模块当前直接 import   │
│  cpp_client，尚未迁入 mapping 层                     │
├─────────────────────────────────────────────────────┤
│  Transport Layer (cpp_client.py)                    │
│  WebSocket 长连接 + 自动重连 + 请求关联 + 事件分发    │
└─────────────────────────────────────────────────────┘

State (state/) 横跨所有层，提供 Session → Project → Module 三级状态
```

## 3. 目录结构与职责

```shell
mcp/
├── main.py                     # 入口：日志→后端连接→事件注册→MCP启动→优雅关闭
├── mcp_server.py               # MCP 协议服务器（3种传输）
├── cpp_client.py               # C++ 后端 WebSocket 客户端
├── config.py                   # 统一配置（pydantic-settings + .env）
├── models.py                   # 协议数据模型（CppRequest/Response/Event）
├── requirements.txt            # Python 依赖
│
├── tools/                      # 工具层：MCP Tool → 业务逻辑
│   ├── __init__.py             #   聚合 ALL_TOOLS + ALL_DISPATCH
│   ├── loader/                 #   导入工具
│   │   ├── tool.py             #     Tool 描述符 + DISPATCH
│   │   ├── handler.py          #     import_trace_file 实现
│   │   └── global_tools.py     #     全局工具（heartbeat + 项目管理，仅 heartbeat 启用）
│   ├── timeline/               #   时间线工具
│   │   ├── tool.py             #     4 个 Tool 描述符 + DISPATCH
│   │   └── handler.py          #     时间线查询实现
│   ├── cluster/                #   聚类/慢卡分析工具
│   │   ├── tool.py             #     3 个 Tool 描述符 + DISPATCH
│   │   └── handler.py          #     慢卡排行、迭代、矩阵组实现
│   ├── operator/               #   算子/内存/汇总工具
│   │   ├── operator.py         #     9 个 handler + 3 个启用 TOOLS/DISPATCH
│   │   └── __init__.py         #     空占位
│   ├── query/                  #   预留（空目录）
│   │   └── __init__.py
│   └── utils/                  #   预留（空目录）
│       └── __init__.py
│
├── mapping/                    # 映射层：C++ API 的唯一封装处
│   ├── __init__.py             #   仅导出 import_trace_file_api
│   ├── framework.py            #   import_trace_file_api
│   └── timeline.py             #   timeline 系列 API 封装
│
├── state/                      # 三级状态管理
│   ├── __init__.py
│   ├── session.py              #   SessionState（项目注册、事件跟踪）
│   ├── project.py              #   ProjectState（导入元数据、模块集合）
│   └── module.py               #   ModuleState（模块级可变字典）
│
├── internal/                   # 内部组件
│   └── profiler_server.py      #   C++ 后端自动启动 & 生命周期管理
│
├── utils/                      # 公共工具
│   ├── __init__.py
│   ├── logger.py               #   loguru 日志（stderr + 滚动文件）
│   ├── errors.py               #   异常体系
│   ├── response.py             #   ok() / err() / fmt_json() 响应格式化
│   └── decorators.py           #   @require_events 前置条件检查
│
├── scenario/                    # 场景模块（fast_slow_rank 子模块已存在）
│   ├── __init__.py
│   └── fast_slow_rank/
│       └── __init__.py
└── __init__.py
```

## 4. 核心组件说明

### 4.1 main.py — 启动入口

```shell
启动流程:
  setup_logger() → start_profiler_server_if_needed() → cpp.initialise()
    → 注册事件监听 → 选择 transport (stdio/sse/websocket) → 运行
```

- 支持 SIGINT 优雅关闭
- 连接失败时不阻塞启动（降级为未连接状态）
- 注册 C++ 后端事件监听：`parse/clusterCompleted`、`parse/clusterStep2Completed` 等

### 4.2 cpp_client.py — 后端通信

- WebSocket 长连接，支持自动重连
- 请求 ID 单调递增，`requestId → Future` 实现并发安全
- `keepalive_loop` 心跳保活，防止 C++ 后端 `idleTimeout` 断开
- 事件分发：注册 `on_event("事件名", handler)`，支持 `"*"` 全局订阅

### 4.3 mapping/ — API 映射层

**设计原则**：理想情况下这是唯一 `import cpp_client` 的地方。所有 tool handlers 不直接调用 `cpp_client.request()`，而是通过本层函数间接通信。

> **注意**：当前 `tools/operator/` 和 `tools/loader/global_tools.py` 模块直接 `import cpp_client`，尚未迁入 mapping 层。后续规划将其 API 封装逐步迁移至 mapping/ 下。

好处：

- 修改 C++ API 协议时只改 mapping 层
- handler 层专注于业务逻辑和响应格式化

### 4.4 state/ — 三级状态

```shell
SessionState (会话级)
  ├── _current_project → ProjectState (项目级)
  │     ├── rank_list / cluster_path / import_result
  │     └── _modules → ModuleState (模块级)
  │           ├── "timeline" → kernel_detail_cache, selected_tid...
  │           └── "cluster" → ...
  └── _completed_events → {"parse/clusterCompleted", ...}
```

- **Session**：管理多个项目、事件完成状态
- **Project**：单个导入的元数据（rank 列表、cluster 路径）
- **Module**：各模块的独立状态（如 timeline 的缓存）

通过全局单例 `state` 访问：

```python
from state import state
ps = state.get_or_create_project("my_proj", "/path/to/data")
ps.get_module("timeline").set("selected_tid", "1234")
```

### 4.5 tools/ — 工具注册

大多数工具模块结构一致：

```shell
tools/<name>/
  ├── tool.py      → TOOLS (MCP 描述符列表) + DISPATCH (名称→handler 映射)
  └── handler.py   → 异步函数实现
```

`tools/loader/global_tools.py` 是例外，将全局管理工具（heartbeat、项目增删等）放在同一模块内。
`tools/operator/operator.py` 将算子、内存、汇总三组 handler 合并在一个文件中。

在 `tools/__init__.py` 中汇总 loader、timeline、cluster 三个模块：

```python
ALL_TOOLS = loader.TOOLS + timeline.TOOLS + cluster.TOOLS
ALL_DISPATCH = {**loader.DISPATCH, **timeline.DISPATCH, **cluster.DISPATCH}
```

> `operator` 模块当前未纳入 `tools/__init__.py`，其工具通过 `operator.py` 中的 TOOLS / DISPATCH 独立管理。

### 4.6 utils/ — 公共工具

| 文件 | 功能 |
|------|------|
| `logger.py` | loguru 配置，双路输出（stderr + 10MB 滚动文件） |
| `errors.py` | 异常体系：`CppBackendError`、`BackendConnectionError`、`RequestTimeoutError`、`NotConnectedError` |
| `response.py` | `ok(body)` 成功响应 / `err(exc)` 错误响应，统一返回 `CallToolResult`；`fmt_json(data)` 序列化 JSON 字符串 |
| `decorators.py` | `@require_events(...)` 检查 C++ 解析事件是否完成，未完成时阻止执行并返回友好提示 |

## 5. 工具能力概览

### 5.1 loader 模块

| 工具名 | 功能 |
|--------|------|
| `import_trace_file` | 导入 trace/profile 文件到 C++ 后端，异步解析 |

### 5.1.1 global 模块（tools/loader/global_tools.py）

| 工具名 | 功能 | 状态 |
|--------|------|------|
| `heartbeat` | 心跳/连通性检查 | 启用 |
| `list_files` | 列出后端主机目录 | 预留 |
| `get_module_config` | 可用分析模块配置 | 预留 |
| `get_project_explorer` | 所有注册项目列表 | 预留 |
| `delete_project_data` | 从项目删除数据路径 | 预留 |
| `clear_projects` | 清空一个或多个项目 | 预留 |
| `check_project_valid` | 验证项目数据路径有效性 | 预留 |
| `rename_project` | 重命名项目 | 预留 |

> global 模块直接调用 `cpp_client`，未经过 mapping 层。

### 5.2 timeline 模块

| 工具名 | 功能 |
|--------|------|
| `query_communication_kernel_detail` | 查询通信算子的 Kernel 级详情 |
| `get_thread_detail` | 获取指定算子的线程详情（依赖 kernel cache） |
| `get_unit_flows` | 获取算子间因果依赖流（依赖 kernel cache + duration） |
| `get_units_in_range` | 获取时间范围内算子列表，可提取特征统计（TOP10/汇总） |

### 5.3 cluster 模块

| 工具名 | 功能 |
|--------|------|
| `duration_iterations` | 获取训练迭代列表，发现可用的 iterationId |
| `matrix_group` | 获取通信矩阵分组，分析通信拓扑 |
| `slow_rank_list` | 按通信时长排序的慢卡排行，定位性能瓶颈 |

所有 cluster 模块工具均有 `@require_events` 前置检查，确保集群解析已完成。

### 5.4 operator 模块（tools/operator/）

**Operator 分析：**

| 工具名 | 功能 | 状态 |
|--------|------|------|
| `get_operator_categories` | 列出算子类别分组 | 预留 |
| `get_operator_statistics` | 聚合算子耗时统计（调用次数、总时长、均值、占比） | 预留 |
| `get_operator_details` | 指定算子的详细调用记录 | 预留 |

**Memory 分析：**

| 工具名 | 功能 | 状态 |
|--------|------|------|
| `get_memory_usage` | 按时间范围查看内存使用，可按资源类型过滤 | 启用 |
| `get_memory_operators` | 按算子维度查看内存分配明细 | 启用 |
| `get_memory_leaks` | MemScope 检测到的内存泄漏块摘要 | 启用 |

**Summary 汇总：**

| 工具名 | 功能 | 状态 |
|--------|------|------|
| `get_summary_top_data` | 返回 TOP-N 热点算子或通信操作 | 预留 |
| `get_summary_statistics` | 整体性能统计面板（计算/通信比、平均步时间等） | 预留 |
| `get_communication_advisor` | AI 生成的通信性能优化建议 | 预留 |

> operator 模块直接调用 `cpp_client`，未经过 mapping 层。

## 6. 关键调用链

### 6.1 Agent 调用 Tool 的链路

```shell
Agent 调用 "query_communication_kernel_detail"
    ↓
mcp_server.py::call_tool() → 查 DISPATCH 表 → timeline.handler
    ↓
handler → mapping.timeline.query_communication_kernel_detail_api()
    ↓
mapping → cpp_client.request("unit/kernelDetail", "timeline", params=...)
    ↓
cpp_client → WebSocket 发送 JSON → C++ 后端
    ↓
C++ 后端返回 → cpp_client 按 requestId 回填 Future
    ↓
mapping 返回 body → handler 格式化 → ok({...}) → CallToolResult
    ↓
返回给 Agent
```

### 6.2 C++ 事件推送链路

```shell
C++ 后端推送 type=event
    ↓
cpp_client receive_loop → 按 event 名分发
    ↓
main.py 注册的监听函数（如 _on_parse_cluster_success）
    ↓
state.mark_event_completed("parse/clusterCompleted", event)
    ↓
后续 @require_events 检查通过，cluster 工具可正常执行
```

## 7. 协议对齐

桥接层与 C++ 后端的 JSON 协议：

**请求**（CppRequest）：

```json
{
  "type": "request",
  "id": 1,
  "command": "unit/kernelDetail",
  "moduleName": "timeline",
  "projectName": "my_project",
  "fileId": "/path/to/file",
  "params": {"rankId": "0", "name": "AllReduce"}
}
```

**响应**（CppResponse）：

```json
{
  "type": "response",
  "id": 2,
  "requestId": 1,
  "result": true,
  "command": "unit/kernelDetail",
  "body": { ... }
}
```

**事件**（CppEvent）：

```json
{
  "type": "event",
  "id": 3,
  "event": "parse/success",
  "moduleName": "global",
  "result": true,
  "body": { ... }
}
```

## 8. 运行方式

### 8.1 安装依赖

```bash
pip install -r requirements.txt
# SSE 需要额外依赖: pip install uvicorn starlette
```

### 8.2 stdio（本地集成，Claude Desktop）

```bash
python main.py
```

### 8.3 SSE（远程 Agent / LangChain）

```bash
MSINSIGHT_MCP_TRANSPORT=sse MSINSIGHT_MCP_PORT=8765 python main.py
```

SSE 地址：`http://127.0.0.1:8765/sse`

### 8.4 WebSocket（原始 WebSocket 客户端）

```bash
MSINSIGHT_MCP_TRANSPORT=websocket MSINSIGHT_MCP_PORT=8765 python main.py
```

WebSocket 地址：`ws://127.0.0.1:8765`

### 8.5 配置项

通过 `MSINSIGHT_` 前缀的环境变量或 `.env` 文件配置：

| 环境变量 | 默认值 | 说明 |
|----------|--------|------|
| `MSINSIGHT_CPP_BACKEND_HOST` | localhost | C++ 后端地址 |
| `MSINSIGHT_CPP_BACKEND_PORT` | 9000 | C++ 后端端口 |
| `MSINSIGHT_MCP_TRANSPORT` | stdio | 传输模式 |
| `MSINSIGHT_MCP_PORT` | 8765 | MCP 端口（sse/ws） |
| `MSINSIGHT_CPP_AUTO_START_BINARY` | 空 | C++ 后端二进制路径（自动启动） |
| `MSINSIGHT_LOG_LEVEL` | INFO | 日志级别 |

## 9. 扩展开发指南

### 9.1 新增工具

1. 在 `tools/<module>/handler.py` 中实现异步 handler 函数
2. 如果是新的 C++ API，先在 `mapping/` 中封装 API 函数
3. 在 `tools/<module>/tool.py` 中添加 Tool 描述符（含 inputSchema / outputSchema）并加入 DISPATCH
4. 确保 `tools/__init__.py` 已 import 该模块
5. 重启服务并通过 `list_tools` 验证

### 9.2 最佳实践

- handler 使用 `ok()` / `err()` 统一返回 `CallToolResult`
- 对依赖前置条件的工具使用 `@require_events` 装饰器
- 通过 `state` 管理跨工具共享状态（如 kernel cache）
- 需要 C++ API 的走 `mapping/` 层，不直接调 `cpp_client`

## 10. 常见问题

### 10.1 SSE 连接报 SSL 错误

现象：`wrong version number`
原因：用 `https://` 访问了 `http://` 服务
解决：改用 `http://127.0.0.1:8765/sse`

### 10.2 工具调用全部失败

- 检查 C++ 后端是否运行：`curl ws://127.0.0.1:9000/`
- 检查 `MSINSIGHT_CPP_BACKEND_HOST/PORT` 是否正确
- 先调 `import_trace_file` 建立项目

### 10.3 cluster 工具返回 "parsing not completed"

- 集群解析是异步的，调用 `import_trace_file` 后需等待
- 监听 `parse/clusterCompleted` 事件完成后方可调用 cluster 工具

### 10.4 get_thread_detail / get_unit_flows 缺少参数

- 这两个工具依赖 `kernel_detail_cache`，必须先调用 `query_communication_kernel_detail`
- `get_unit_flows` 还依赖 `get_thread_detail` 获取 duration

## 11. 安全与部署

- 生产环境不要直接暴露在公网
- 公网访问建议：网关鉴权、HTTPS 证书、IP 白名单、速率限制
- SSE 当前为 HTTP 明文，如需 HTTPS 建议通过 Nginx 反代
