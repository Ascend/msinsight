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
    net::{Ipv4Addr, SocketAddrV4, TcpListener},
    path::PathBuf,
    process::Command,
};

use crate::webview;

const SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "profiler_server"];
const ACP_SERVER_RELATIVE_LIST: [&str; 4] =
    ["resources", "profiler", "server", "insight_web_agent"];
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

fn run_server(root_path: &PathBuf, cache_path: &PathBuf, port: u16) {
    let binding = server_path(root_path).unwrap();
    let Some(path) = binding.to_str() else { unreachable!() };

    let mut server_command = Command::new(path);

    #[cfg(windows)]
    server_command.creation_flags(NO_WINDOW_FLAG);

    // 通过Rust底座拉起后端时，为本地使用场景，不涉及远程通信，传入--notStrict选项，导入文件时不要求权限和属主校验通过
    match server_command
        .arg(format!("--wsPort={port}"))
        .arg(format!("--logPath={}", cache_path.display()))
        .arg("--notStrict")
        .spawn()
    {
        Ok(child) => unsafe {
            PID = child.id();
        },
        _ => eprintln!("Failed to start server"),
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

fn run_acp_server(root_path: &PathBuf, port: u16) {
    let Some(acp_path) = acp_server_path(root_path) else {
        eprintln!("ACP node server path does not exist");
        return;
    };
    let entry_path = acp_path.join("index.mjs");

    let mut server_command = Command::new("node");

    #[cfg(windows)]
    server_command.creation_flags(NO_WINDOW_FLAG);

    match server_command
        .arg(entry_path)
        .arg("--path")
        .arg(acp_path)
        .arg("--port")
        .arg(port.to_string())
        .spawn()
    {
        Ok(child) => unsafe {
            ACP_PID = child.id();
        },
        _ => eprintln!("Failed to start ACP node server"),
    }
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
    for port in start..=end {
        let addr = SocketAddrV4::new(Ipv4Addr::new(127, 0, 0, 1), port);
        if TcpListener::bind(addr).is_ok() {
            return Some(port);
        }
    }

    None
}

pub static mut PID: u32 = u32::MAX;
pub static mut ACP_PID: u32 = u32::MAX;

pub fn main() {
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

    let Some(acp_port) = find_first_available_port(port + 1, 9100) else {
        eprintln!("No available ACP port between 9000 and 9100");
        return;
    };

    if let Ok((eventloop, webview)) = webview::run_script(&root_path, &cache_path, port, acp_port) {
        run_server(&root_path, &cache_path, port);
        run_acp_server(&root_path, acp_port);
        webview::run_event_loop(eventloop, webview)
    }
}
