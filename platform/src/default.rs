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
    ffi::OsStr,
    net::{Ipv4Addr, Ipv6Addr, SocketAddrV4, SocketAddrV6, TcpListener},
    path::PathBuf,
    process::Command,
};
#[cfg(target_os = "macos")]
use std::{
    io::{self, Read},
    process::{Child, Stdio},
    thread,
    time::{Duration, Instant},
};

use crate::webview;

const SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "profiler_server"];
const ACP_SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "insight_web_agent"];
const MINIMUM_NODE_VERSION: (u32, u32, u32) = (22, 14, 0);
#[cfg(windows)]
const NO_WINDOW_FLAG: u32 = 0x08000000;

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

fn run_server(root_path: &PathBuf, cache_path: &PathBuf, port: u16) -> bool {
    let Some(path) = server_path(root_path) else {
        eprintln!("Profiler server path does not exist");
        return false;
    };

    let mut server_command = Command::new(path);

    #[cfg(windows)]
    server_command.creation_flags(NO_WINDOW_FLAG);

    // 通过Rust底座拉起后端时，为本地使用场景，不涉及远程通信，传入--notStrict选项，导入文件时不要求权限和属主校验通过
    server_command
        .arg(format!("--wsPort={port}"))
        .arg(format!("--logPath={}", cache_path.display()))
        .arg("--notStrict");
    spawn_profiler_server(&mut server_command)
}

fn spawn_profiler_server(server_command: &mut Command) -> bool {
    match server_command.spawn() {
        Ok(child) => unsafe {
            PID = child.id();
            true
        },
        Err(error) => {
            eprintln!("Failed to start profiler server: {error}");
            false
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

fn node_version_supported() -> bool {
    let Ok(output) = Command::new("node").arg("--version").output() else {
        eprintln!("Node executable was not found in PATH");
        return false;
    };
    let version = String::from_utf8_lossy(&output.stdout);
    let supported = output.status.success() && parse_node_version(&version)
        .is_some_and(|version| version >= MINIMUM_NODE_VERSION);
    if !supported {
        eprintln!("ACP requires Node.js 22.14.0 or later");
    }
    supported
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

fn find_first_available_port(start: u16, end: u16) -> Option<u16> {
    // 探测系统是否支持 IPv6 loopback 绑定：若不支持则仅检测 IPv4，避免在禁用 IPv6 的环境失效
    let ipv6_capable =
        TcpListener::bind(SocketAddrV6::new(Ipv6Addr::LOCALHOST, 0, 0, 0)).is_ok();

    for port in start..=end {
        let v4_ok = TcpListener::bind(SocketAddrV4::new(Ipv4Addr::new(127, 0, 0, 1), port))
            .is_ok();
        // IPv6 不可用时跳过 v6 检测，等价于仅检测 IPv4，保证兼容性
        let v6_ok = if ipv6_capable {
            TcpListener::bind(SocketAddrV6::new(Ipv6Addr::LOCALHOST, port, 0, 0)).is_ok()
        } else {
            true
        };
        if v4_ok && v6_ok {
            return Some(port);
        }
    }

    None
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

    if webview::webview_version().is_err() {
        #[cfg(windows)]
        webview::webview2err::show_webview_err_message();

        return;
    }

    let Some(port) = find_first_available_port(9000, 9100) else {
        eprintln!("No available port between 9000 and 9100");
        return;
    };

    let Some(acp_port) = find_first_available_port(port.saturating_add(1), 9100) else {
        eprintln!("No available ACP port between 9000 and 9100");
        return;
    };
    if !node_version_supported() {
        return;
    }
    let Some(capability_token) = generate_capability_token() else {
        return;
    };

    if !run_server(&root_path, &cache_path, port) {
        return;
    }
    if !run_acp_server(&root_path, &cache_path, acp_port, &capability_token) {
        webview::cleanup::handle_close_requested();
        return;
    }

    #[cfg(target_os = "linux")]
    if let Ok((eventloop, webview)) = webview::run_script(&root_path, &cache_path, port, acp_port, &capability_token) {
        webview::run_event_loop(eventloop, webview)
    }

    #[cfg(any(target_os = "windows", target_os = "macos"))]
    if let Ok((eventloop, webview, window)) = webview::run_script(&root_path, &cache_path, port, acp_port, &capability_token) {
        webview::run_event_loop(eventloop, webview, window)
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
    use super::{parse_node_version, spawn_profiler_server, MINIMUM_NODE_VERSION};

    #[test]
    fn enforces_blade_node_runtime_floor() {
        assert!(parse_node_version("v22.14.0").unwrap() >= MINIMUM_NODE_VERSION);
        assert!(parse_node_version("v22.13.1").unwrap() < MINIMUM_NODE_VERSION);
        assert_eq!(parse_node_version("not-node"), None);
    }

    #[test]
    fn profiler_spawn_failure_is_reported() {
        let mut command = Command::new("mindstudio-profiler-server-does-not-exist");

        assert!(!spawn_profiler_server(&mut command));
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
