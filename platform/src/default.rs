/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#[cfg(windows)]
use std::os::windows::process::CommandExt;
use std::{
    env,
    env::current_exe,
    net::{SocketAddr, TcpStream},
    path::PathBuf,
    process::{Child, Command, Stdio},
    thread,
    time::{Duration, Instant},
};
#[cfg(target_os = "macos")]
use std::{
    ffi::OsStr,
    io::{self, Read},
};

#[cfg(target_os = "linux")]
use crate::local_port::dialog_tool_was_shown;
use crate::{
    local_port::{
        find_first_available_port, no_available_port_dialog_message,
        no_available_port_stderr, server_is_ready, FRONTEND_BACKEND_PORT_END,
        FRONTEND_BACKEND_PORT_START, NO_AVAILABLE_PORT_DIALOG_TITLE,
    },
    webview,
};

const SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "profiler_server"];
const ACP_SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "insight_web_agent"];
const MINIMUM_NODE_VERSION: (u32, u32, u32) = (22, 14, 0);
const ACP_READY_TIMEOUT: Duration = Duration::from_secs(12);
#[cfg(windows)]
const NO_WINDOW_FLAG: u32 = 0x08000000;
const BACKEND_READY_TIMEOUT: Duration = Duration::from_secs(10);
const BACKEND_READY_POLL_INTERVAL: Duration = Duration::from_millis(20);
const BACKEND_START_FAILED_MESSAGE: &str =
    "MindStudio Insight backend failed to start. Please restart the application.";

struct AcpLaunch {
    status: &'static str,
    port: u16,
    token: String,
    node_version: String,
}

impl AcpLaunch {
    fn failed(status: &'static str, node_version: String) -> Self {
        Self {
            status,
            port: 0,
            token: String::new(),
            node_version,
        }
    }
}

enum NodeResolve {
    Found((u32, u32, u32)),
    Missing,
    Unsupported(String),
}

fn server_path(root_path: &PathBuf) -> Option<PathBuf> {
    let mut server_path = root_path.to_path_buf();

    #[cfg(target_os = "macos")]
    {
        env::set_var(
            "DYLD_LIBRARY_PATH",
            server_path
                .join("resources/profiler/server/:$DYLD_LIBRARY_PATH")
                .as_path(),
        );
    }

    #[cfg(target_os = "linux")]
    {
        let ld_library_path =
            env::var_os("LD_LIBRARY_PATH").unwrap_or_default();
        let new_ld_library_path = format!(
            "{}:{}",
            server_path.join("resources/profiler/server/").display(),
            ld_library_path.to_string_lossy()
        );
        env::set_var("LD_LIBRARY_PATH", &new_ld_library_path);
    }

    for tmp in SERVER_RELATIVE_LIST {
        server_path.push(tmp);
    }

    #[cfg(windows)]
    server_path.set_extension("exe");

    if !server_path.exists() {
        return None;
    }

    Some(server_path)
}

fn terminate_server(child: &mut Child) {
    let pid = child.id();
    tracing::info!(
        target: "msinsight_platform",
        event = "backend_stop_requested",
        pid
    );
    let kill_result = child.kill();
    let wait_result = child.wait();
    // u32::MAX 表示当前没有可清理的后端进程，避免关闭流程重复使用旧 PID。
    unsafe {
        PID = u32::MAX;
    }

    match (kill_result, wait_result) {
        (Ok(()), Ok(status)) => tracing::info!(
            target: "msinsight_platform",
            event = "backend_stopped",
            pid,
            status = %status
        ),
        (Err(error), _) => tracing::error!(
            target: "msinsight_platform",
            event = "backend_stop_failed",
            pid,
            reason = "kill_failed",
            error = %error
        ),
        (_, Err(error)) => tracing::error!(
            target: "msinsight_platform",
            event = "backend_stop_failed",
            pid,
            reason = "wait_failed",
            error = %error
        ),
    }
}

struct ServerProcessGuard {
    child: Child,
}

impl ServerProcessGuard {
    fn new(child: Child) -> Self {
        Self { child }
    }

    fn child_mut(&mut self) -> &mut Child {
        &mut self.child
    }
}

impl Drop for ServerProcessGuard {
    fn drop(&mut self) {
        // Child's Drop does not stop the process, so keep this guard armed
        // while the WebView is built and throughout the event loop.
        terminate_server(&mut self.child);
    }
}

fn run_server(
    root_path: &PathBuf,
    cache_path: &PathBuf,
    port: u16,
) -> Option<ServerProcessGuard> {
    let Some(path) = server_path(root_path) else {
        tracing::error!(
            target: "msinsight_platform",
            event = "backend_start_failed",
            port,
            reason = "executable_not_found"
        );
        return None;
    };

    let mut server_command = Command::new(path);

    #[cfg(windows)]
    server_command.creation_flags(NO_WINDOW_FLAG);

    // 通过Rust底座拉起后端时，为本地使用场景，不涉及远程通信，传入--notStrict选项，导入文件时不要求权限和属主校验通过
    server_command
        .arg(format!("--wsPort={port}"))
        .arg(format!("--logPath={}", cache_path.display()))
        .arg("--notStrict");

    let mut server_process = spawn_profiler_server(&mut server_command, port)?;
    let started = Instant::now();
    loop {
        match server_process.child_mut().try_wait() {
            Ok(Some(status)) => {
                tracing::error!(
                    target: "msinsight_platform",
                    event = "backend_start_failed",
                    port,
                    reason = "exited_before_ready",
                    status = %status
                );
                return None;
            }
            Err(_) => {
                tracing::error!(
                    target: "msinsight_platform",
                    event = "backend_start_failed",
                    port,
                    reason = "status_query_failed"
                );
                return None;
            }
            Ok(None) => {}
        }

        if server_is_ready(port) {
            // A TCP listener alone cannot prove ownership of the port. Check
            // the spawned backend again before allowing the WebView to start.
            match server_process.child_mut().try_wait() {
                Ok(None) => {
                    tracing::info!(
                        target: "msinsight_platform",
                        event = "backend_ready",
                        port,
                        elapsed_ms = started.elapsed().as_millis() as u64
                    );
                    return Some(server_process);
                }
                Ok(Some(status)) => {
                    tracing::error!(
                        target: "msinsight_platform",
                        event = "backend_start_failed",
                        port,
                        reason = "exited_after_readiness_probe",
                        status = %status
                    );
                    return None;
                }
                Err(_) => {
                    tracing::error!(
                        target: "msinsight_platform",
                        event = "backend_start_failed",
                        port,
                        reason = "status_query_failed_after_readiness_probe"
                    );
                    return None;
                }
            }
        }

        if started.elapsed() >= BACKEND_READY_TIMEOUT {
            tracing::error!(
                target: "msinsight_platform",
                event = "backend_start_failed",
                port,
                reason = "readiness_timeout"
            );
            return None;
        }

        thread::sleep(BACKEND_READY_POLL_INTERVAL);
    }
}

fn spawn_profiler_server(
    server_command: &mut Command,
    port: u16,
) -> Option<ServerProcessGuard> {
    tracing::info!(
        target: "msinsight_platform",
        event = "backend_start_requested",
        port
    );
    match server_command.spawn() {
        Ok(child) => {
            tracing::info!(
                target: "msinsight_platform",
                event = "backend_started",
                port,
                pid = child.id()
            );
            unsafe {
                PID = child.id();
            }
            Some(ServerProcessGuard::new(child))
        }
        Err(_) => {
            tracing::error!(
                target: "msinsight_platform",
                event = "backend_start_failed",
                port,
                reason = "spawn_failed"
            );
            eprintln!("Failed to start server");
            None
        }
    }
}

fn acp_server_path(root_path: &PathBuf) -> Option<PathBuf> {
    let mut server_path = root_path.to_path_buf();
    for tmp in ACP_SERVER_RELATIVE_LIST {
        server_path.push(tmp);
    }

    if !server_path.join("index.mjs").exists() {
        return None;
    }

    Some(server_path)
}

fn run_acp_server(
    root_path: &PathBuf,
    cache_path: &PathBuf,
    port: u16,
    capability_token: &str,
) -> bool {
    let Some(acp_path) = acp_server_path(root_path) else {
        eprintln!("ACP node server path does not exist");
        return false;
    };
    let entry_path = acp_path.join("index.mjs");

    let mut server_command = Command::new("node");

    #[cfg(windows)]
    server_command.creation_flags(NO_WINDOW_FLAG);

    server_command
        .arg(entry_path)
        .arg("--path")
        .arg(cache_path)
        .arg("--resource-path")
        .arg(acp_path)
        .arg("--port")
        .arg(port.to_string())
        .arg("--host")
        .arg("127.0.0.1");

    #[cfg(target_os = "macos")]
    server_command
        .arg("--allowed-origin")
        .arg("wry://localhost");
    #[cfg(windows)]
    server_command
        .arg("--allowed-origin")
        .arg("http://wry.localhost");
    #[cfg(target_os = "linux")]
    // Legacy Wry omits Origin on Linux but still requires a CORS response.
    server_command
        .arg("--allowed-origin")
        .arg("wry://localhost")
        .arg("--allowed-origin")
        .arg("*");

    server_command.env("ACP_CAPABILITY_TOKEN", capability_token);

    match server_command.spawn() {
        Ok(child) => unsafe {
            ACP_PID = child.id();
            true
        },
        Err(error) => {
            eprintln!("Failed to start ACP node server: {error}");
            false
        }
    }
}

fn parse_node_version(version: &str) -> Option<(u32, u32, u32)> {
    let parts: Vec<u32> = version
        .trim()
        .trim_start_matches('v')
        .split('.')
        .take(3)
        .filter_map(|part| part.parse().ok())
        .collect();
    (parts.len() == 3).then(|| (parts[0], parts[1], parts[2]))
}

fn format_node_version(version: (u32, u32, u32)) -> String {
    format!("{}.{}.{}", version.0, version.1, version.2)
}

fn inspect_node_runtime() -> NodeResolve {
    let Ok(output) = Command::new("node").arg("--version").output() else {
        eprintln!("Node executable was not found in PATH");
        return NodeResolve::Missing;
    };
    if !output.status.success() {
        eprintln!("Node executable was not found in PATH");
        return NodeResolve::Missing;
    }
    let version = String::from_utf8_lossy(&output.stdout);
    match parse_node_version(&version) {
        Some(parsed) if parsed >= MINIMUM_NODE_VERSION => NodeResolve::Found(parsed),
        Some(parsed) => {
            let formatted = format_node_version(parsed);
            eprintln!("ACP requires Node.js 22.14.0 or later; found {formatted}");
            NodeResolve::Unsupported(formatted)
        }
        None => {
            eprintln!("ACP requires Node.js 22.14.0 or later");
            NodeResolve::Unsupported(String::new())
        }
    }
}

fn wait_for_tcp(port: u16, timeout: Duration) -> bool {
    let addr = SocketAddr::from(([127, 0, 0, 1], port));
    let deadline = Instant::now() + timeout;
    while Instant::now() < deadline {
        if TcpStream::connect_timeout(&addr, Duration::from_millis(200)).is_ok() {
            return true;
        }
        thread::sleep(Duration::from_millis(100));
    }
    false
}

fn kill_pid(pid: u32) {
    if pid == u32::MAX {
        return;
    }
    let pid = pid.to_string();
    #[cfg(windows)]
    {
        let mut command = Command::new("taskkill");
        command
            .arg("/f")
            .arg("/t")
            .arg("/pid")
            .arg(&pid)
            .creation_flags(NO_WINDOW_FLAG)
            .stdout(Stdio::null())
            .stderr(Stdio::null());
        let _ = command.output();
    }
    #[cfg(unix)]
    {
        let _ = Command::new("kill")
            .arg("-9")
            .arg(&pid)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status();
    }
}

fn start_acp_sidecar(
    root_path: &PathBuf,
    cache_path: &PathBuf,
    profiler_port: u16,
) -> AcpLaunch {
    match inspect_node_runtime() {
        NodeResolve::Missing => AcpLaunch::failed("missing-node", String::new()),
        NodeResolve::Unsupported(version) => AcpLaunch::failed("unsupported-node", version),
        NodeResolve::Found(version) => {
            let node_version = format_node_version(version);
            if acp_server_path(root_path).is_none() {
                eprintln!("ACP node server path does not exist");
                return AcpLaunch::failed("start-failed", node_version);
            }
            let Some(acp_port) =
                find_first_available_port(profiler_port.saturating_add(1), 9100)
            else {
                eprintln!("No available ACP port between 9000 and 9100");
                return AcpLaunch::failed("start-failed", node_version);
            };
            let Some(token) = generate_capability_token() else {
                return AcpLaunch::failed("start-failed", node_version);
            };
            if !run_acp_server(root_path, cache_path, acp_port, &token) {
                return AcpLaunch::failed("start-failed", node_version);
            }
            if !wait_for_tcp(acp_port, ACP_READY_TIMEOUT) {
                eprintln!("ACP node server did not become ready on port {acp_port}");
                unsafe {
                    kill_pid(ACP_PID);
                    ACP_PID = u32::MAX;
                }
                return AcpLaunch::failed("start-failed", node_version);
            }
            AcpLaunch {
                status: "ready",
                port: acp_port,
                token,
                node_version,
            }
        }
    }
}

fn generate_capability_token() -> Option<String> {
    let mut bytes = [0_u8; 32];
    if let Err(error) = getrandom::fill(&mut bytes) {
        eprintln!("Failed to generate ACP capability token: {error}");
        return None;
    }
    Some(bytes.iter().map(|byte| format!("{byte:02x}")).collect())
}

fn home_dir() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    {
        env::var("USERPROFILE").ok().map(PathBuf::from)
    }

    #[cfg(not(target_os = "windows"))]
    {
        env::var("HOME").ok().map(PathBuf::from)
    }
}

#[cfg(windows)]
#[link(name = "shell32")]
extern "system" {
    /// Tests whether the current user is a member of the Administrator's group.
    ///
    /// ### FFI Signature
    /// ```c++
    /// BOOL IsUserAnAdmin();
    /// ```
    pub fn IsUserAnAdmin() -> bool;
}

#[cfg(windows)]
fn is_admin() -> bool {
    /// ### Safety
    /// No any Memory Safety problems
    unsafe {
        IsUserAnAdmin()
    }
}

#[cfg(windows)]
fn eq_prefix(lhs: &PathBuf, rhs: &PathBuf) -> bool {
    match (lhs.components().next(), rhs.components().next()) {
        (Some(l), Some(r)) => l == r,
        _ => false,
    }
}

#[cfg(target_os = "macos")]
fn load_login_shell_environment() {
    let shell = env::var("SHELL")
        .ok()
        .filter(|shell| PathBuf::from(shell).exists())
        .unwrap_or_else(|| "/bin/zsh".to_string());
    let home = home_dir().unwrap_or_else(|| PathBuf::from("/"));
    let home_arg = shell_single_quote(home.as_os_str());
    let user = env::var("USER").unwrap_or_default();
    let logname = env::var("LOGNAME").unwrap_or_else(|_| user.clone());
    let command = format!("cd {home_arg}; env -0");

    let mut shell_command = Command::new("/usr/bin/env");
    shell_command
        .arg("-i")
        .arg(format!("HOME={}", home.display()))
        .arg(format!("USER={user}"))
        .arg(format!("LOGNAME={logname}"))
        .arg("PATH=/usr/bin:/bin:/usr/sbin:/sbin")
        .arg(format!("SHELL={shell}"))
        .arg(&shell)
        .arg("-l")
        .arg("-i")
        .arg("-c")
        .arg(command)
        .stdout(Stdio::piped())
        .stderr(Stdio::null());

    let output = shell_command.spawn().and_then(|child| {
        wait_for_child_output(child, Duration::from_secs(10))
    });

    match output {
        Ok(Some(output)) => {
            for entry in output.split(|byte| *byte == 0) {
                if entry.is_empty() {
                    continue;
                }
                if let Some(index) = entry.iter().position(|byte| *byte == b'=')
                {
                    let name = String::from_utf8_lossy(&entry[..index]);
                    if name == "SHLVL" || name.is_empty() {
                        continue;
                    }
                    let value = String::from_utf8_lossy(&entry[index + 1..]);
                    env::set_var(name.as_ref(), value.as_ref());
                }
            }
            eprintln!("Loaded login shell environment from {shell}");
        }
        Ok(None) => {
            eprintln!("Timed out loading login shell environment; using inherited environment")
        }
        Err(error) => {
            eprintln!("Failed to load login shell environment: {error}")
        }
    }
}

#[cfg(target_os = "macos")]
fn wait_for_child_output(
    mut child: Child,
    timeout: Duration,
) -> io::Result<Option<Vec<u8>>> {
    let deadline = Instant::now() + timeout;
    loop {
        if let Some(status) = child.try_wait()? {
            if !status.success() {
                return Ok(None);
            }
            let mut output = Vec::new();
            if let Some(mut stdout) = child.stdout.take() {
                stdout.read_to_end(&mut output)?;
            }
            return Ok(Some(output));
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            child.wait()?;
            return Ok(None);
        }
        thread::sleep(Duration::from_millis(50));
    }
}

#[cfg(target_os = "macos")]
fn shell_single_quote(value: &OsStr) -> String {
    let value = value.to_string_lossy();
    format!("'{}'", value.replace('\'', "'\\''"))
}

pub static mut PID: u32 = u32::MAX;
pub static mut ACP_PID: u32 = u32::MAX;

pub fn main() {
    #[cfg(target_os = "macos")]
    load_login_shell_environment();

    let mut cache_path = home_dir()
        .expect("Home dir not exists, check your env variable")
        .join(".mindstudio_insight"); //cache folder generated for each user.
    let root_path = current_exe()
        .expect("Failed to get current exe path")
        .parent()
        .expect("Failed to get parent path of  current exe")
        .to_path_buf();

    #[cfg(windows)]
    {
        // 当用户安装在C盘时，使用user目录
        let mut webview_path = cache_path.clone();
        // 当用户安装在C盘外时，使用安装目录
        if !eq_prefix(&cache_path, &root_path) {
            cache_path = root_path.join(".mindstudio_insight");
            webview_path = cache_path.clone();
        }
        if is_admin() {
            webview_path.push("admin");
        }
        env::set_var("WEBVIEW2_USER_DATA_FOLDER", webview_path.as_path());
    }

    if !cache_path.exists() {
        #[cfg(windows)]
        {
            use std::fs::create_dir_all;

            create_dir_all(cache_path.as_path())
                .expect("no permission to create cache_path");
        }

        #[cfg(unix)]
        {
            use std::{fs::DirBuilder, os::unix::fs::DirBuilderExt};

            DirBuilder::new()
                .recursive(true)
                .mode(0o750)
                .create(cache_path.as_path())
                .expect("no permission to create cache_path");
        }
    }

    // cache_path 已完成平台修正且目录已创建；此时初始化可保证 Rust 与后端日志共用该目录。
    // initialize 只安装一次进程级 tracing subscriber，后续各模块直接调用 tracing::* 即会经过
    // 该 subscriber 过滤并写入，无需重复初始化。
    // 可选控制句柄只交给当前平台的 WebView；初始化失败时保留 None，维持应用尽力启动的边界。
    let log_level_control = match crate::logging::initialize(&cache_path) {
        Ok(control) => Some(control),
        Err(_) => {
            eprintln!("Failed to initialize Rust logging");
            None
        }
    };
    tracing::info!(target: "msinsight_platform", event = "app_start");

    if webview::webview_version().is_err() {
        tracing::error!(
            target: "msinsight_platform",
            event = "webview_version_query_failed"
        );
        #[cfg(windows)]
        webview::webview2err::show_webview_err_message();

        return;
    }

    let Some(port) = find_first_available_port(
        FRONTEND_BACKEND_PORT_START,
        FRONTEND_BACKEND_PORT_END,
    ) else {
        tracing::error!(
            target: "msinsight_platform",
            event = "backend_port_unavailable",
            reason = "no_available_port"
        );
        eprintln!(
            "{}",
            no_available_port_stderr(
                FRONTEND_BACKEND_PORT_START,
                FRONTEND_BACKEND_PORT_END
            )
        );
        show_no_available_port_alert();
        return;
    };

    let Some(server_process) = run_server(&root_path, &cache_path, port) else {
        show_backend_start_failed_alert();
        return;
    };

    let acp = start_acp_sidecar(&root_path, &cache_path, port);

    #[cfg(target_os = "linux")]
    {
        tracing::info!(target: "msinsight_platform", event = "webview_start");
        match webview::run_script(
            &root_path,
            &cache_path,
            port,
            acp.port,
            &acp.token,
            acp.status,
            &acp.node_version,
            log_level_control,
        ) {
            Ok((eventloop, webview)) => {
                tracing::info!(
                    target: "msinsight_platform",
                    event = "webview_created"
                );
                // Keep cleanup armed while the event loop owns the UI.
                let _server_process = server_process;
                webview::run_event_loop(eventloop, webview)
            }
            Err(_) => {
                tracing::error!(
                    target: "msinsight_platform",
                    event = "webview_start_failed",
                    reason = "build_failed"
                );
            }
        }
    }

    #[cfg(any(target_os = "windows", target_os = "macos"))]
    {
        tracing::info!(target: "msinsight_platform", event = "webview_start");
        match webview::run_script(
            &root_path,
            &cache_path,
            port,
            acp.port,
            &acp.token,
            acp.status,
            &acp.node_version,
            log_level_control,
        ) {
            Ok((eventloop, webview, window)) => {
                tracing::info!(
                    target: "msinsight_platform",
                    event = "webview_created"
                );
                // Keep cleanup armed while the event loop owns the UI.
                let _server_process = server_process;
                webview::run_event_loop(eventloop, webview, window)
            }
            Err(_) => {
                tracing::error!(
                    target: "msinsight_platform",
                    event = "webview_start_failed",
                    reason = "build_failed"
                );
            }
        }
    }

    webview::cleanup::handle_close_requested();
}

#[cfg(test)]
mod tests {
    use std::process::Command;

    #[cfg(target_os = "macos")]
    use std::{
        process::Stdio,
        time::{Duration, Instant},
    };

    #[cfg(target_os = "macos")]
    use super::wait_for_child_output;
    use super::{
        parse_node_version, spawn_profiler_server, wait_for_tcp, AcpLaunch,
        MINIMUM_NODE_VERSION,
    };

    #[test]
    fn failed_acp_launch_preserves_status_without_connection_credentials() {
        for (status, node_version) in [
            ("missing-node", ""),
            ("unsupported-node", "22.13.1"),
            ("start-failed", "22.14.0"),
        ] {
            let launch = AcpLaunch::failed(status, node_version.to_string());
            assert_eq!(launch.status, status);
            assert_eq!(launch.node_version, node_version);
            assert_eq!(launch.port, 0);
            assert!(launch.token.is_empty());
        }
    }

    #[test]
    fn enforces_node_runtime_floor() {
        assert!(parse_node_version("v22.14.0").unwrap() >= MINIMUM_NODE_VERSION);
        assert!(parse_node_version("v22.13.1").unwrap() < MINIMUM_NODE_VERSION);
        assert_eq!(parse_node_version("not-node"), None);
    }

    #[test]
    fn wait_for_tcp_detects_listening_port() {
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        assert!(wait_for_tcp(port, std::time::Duration::from_secs(1)));
        drop(listener);
        assert!(!wait_for_tcp(port, std::time::Duration::from_millis(200)));
    }

    #[test]
    fn profiler_spawn_failure_is_reported() {
        let mut command = Command::new("mindstudio-profiler-server-does-not-exist");

        assert!(spawn_profiler_server(&mut command, 0).is_none());
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn timed_out_child_is_killed_and_reaped() {
        let mut command = Command::new("/bin/sh");
        let child = command
            .arg("-c")
            .arg("while :; do :; done")
            .stdout(Stdio::piped())
            .spawn()
            .unwrap();
        let started = Instant::now();

        assert_eq!(
            wait_for_child_output(child, Duration::from_millis(20)).unwrap(),
            None
        );
        assert!(started.elapsed() < Duration::from_secs(2));
    }
}

fn show_no_available_port_alert() {
    let message = no_available_port_dialog_message(
        FRONTEND_BACKEND_PORT_START,
        FRONTEND_BACKEND_PORT_END,
    );
    show_startup_alert(&message);
}

fn show_backend_start_failed_alert() {
    show_startup_alert(BACKEND_START_FAILED_MESSAGE);
}

fn show_startup_alert(message: &str) {
    let title = NO_AVAILABLE_PORT_DIALOG_TITLE;

    #[cfg(windows)]
    webview::webview2err::show_error_message(title, message);

    #[cfg(target_os = "macos")]
    show_macos_error_dialog(title, message);

    #[cfg(target_os = "linux")]
    show_linux_error_dialog(title, message);
}

#[cfg(target_os = "macos")]
fn apple_script_literal(text: &str) -> String {
    let escaped = text.replace('\\', "\\\\").replace('"', "\\\"");
    format!("\"{}\"", escaped.replace('\n', "\" & return & \""))
}

#[cfg(target_os = "macos")]
fn show_macos_error_dialog(title: &str, message: &str) {
    let script = format!(
        "display dialog {} with title {} buttons {{\"OK\"}} \
         default button \"OK\" with icon stop",
        apple_script_literal(message),
        apple_script_literal(title),
    );
    let _ = Command::new("osascript").arg("-e").arg(script).status();
}

#[cfg(target_os = "linux")]
fn dialog_tool_started(command: &mut Command) -> bool {
    dialog_tool_was_shown(command.status())
}

#[cfg(target_os = "linux")]
fn show_linux_error_dialog(title: &str, message: &str) {
    if dialog_tool_started(Command::new("zenity").args([
        "--error",
        "--title",
        title,
        "--text",
        message,
        "--no-markup",
        "--width=480",
    ])) {
        return;
    }
    if dialog_tool_started(
        Command::new("kdialog")
            .args(["--title", title, "--error", message]),
    ) {
        return;
    }
    let _ = Command::new("xmessage")
        .args(["-center", "-title", title, message])
        .status();
}
