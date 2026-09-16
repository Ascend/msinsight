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

#[path = "webview/cleanup.rs"]
mod cleanup;

use crate::logging::RustLogLevel;
use std::path::Path;
use std::{fs::read, path::PathBuf, process::Command, sync::Arc};

pub use wry024::webview::webview_version;
use wry024::{
    application::{
        event::{Event, WindowEvent},
        event_loop::{ControlFlow, EventLoop, EventLoopProxy},
        window::{Window, WindowBuilder},
    },
    http::{header::CONTENT_TYPE, Response},
    webview::{FileDropEvent, WebView, WebViewBuilder},
};

const MIMETYPE_HTML: &str = "text/html";

// IPC 文本解析保留在 WebView 业务边界，日志模块只接收受限枚举，不感知原始文本。
// 仅允许两条精确消息以避免扩大控制面；拒绝内容和原始 IPC 输入绝不持久化。
fn rust_log_level_from_ipc(front_end_msg: &str) -> Option<RustLogLevel> {
    match front_end_msg {
        "setRustLogLevel|DEBUG" => Some(RustLogLevel::Debug),
        "setRustLogLevel|INFO" => Some(RustLogLevel::Info),
        _ => None,
    }
}

// 仅记录前端静态资源名；目录跳转和异常字符不能进入日志。
fn resource_name_for_log(path: &str) -> Option<&str> {
    let relative = path.strip_prefix("/resources/profiler/frontend/")?;
    if !relative
        .bytes()
        .all(|byte| byte.is_ascii_alphanumeric() || b"/._-".contains(&byte))
        || relative
            .split('/')
            .any(|part| matches!(part, "" | "." | ".."))
    {
        return None;
    }
    relative.rsplit('/').next()
}

fn create_webview(
    window: Window,
    cache_path: Arc<PathBuf>,
    resource_path: Arc<PathBuf>,
    port: u16,
    proxy: Arc<EventLoopProxy<PathBuf>>,
    log_level_control: Option<crate::logging::LogLevelControl>,
) -> wry024::Result<WebView> {
    WebViewBuilder::new(window)?
        .with_custom_protocol("wry".into(), move |request| {
            let path = request.uri().path();
            let resource_name =
                resource_name_for_log(path).unwrap_or("<redacted>");
            let content = match read(resource_path.join(&path[1..]).as_path()) {
                Ok(content) => content.into(),
                Err(e) => {
                    tracing::warn!(
                        target: "msinsight_platform",
                        event = "resource_read_failed",
                        resource_name,
                        reason = "read_failed"
                    );
                    return Err(wry024::Error::Io(e));
                }
            };

            let mimetype = extract_mimetype(path);
            tracing::debug!(
                target: "msinsight_platform",
                event = "resource_read_succeeded",
                resource_name,
                response_mime = mimetype
            );

            Response::builder()
                .header(CONTENT_TYPE, mimetype)
                .body(content)
                .map_err(Into::into)
        })
        .with_url(
            format!(
                "wry://localhost/resources/profiler/frontend/index.html?port={}",
                port
            )
            .as_str(),
        )?
        .with_file_drop_handler(move |_, ev| {
            match ev {
                FileDropEvent::Dropped(paths) => {
                    tracing::debug!(
                        target: "msinsight_platform",
                        event = "file_drop_received",
                        file_count = paths.len()
                    );
                    match proxy.send_event(paths[0].to_owned()) {
                        Ok(()) => tracing::debug!(
                            target: "msinsight_platform",
                            event = "file_drop_event_sent"
                        ),
                        Err(_) => tracing::warn!(
                            target: "msinsight_platform",
                            event = "file_drop_event_send_failed",
                            reason = "send_failed"
                        ),
                    }
                }
                _ => {}
            }

            true
        })
        .with_ipc_handler(move |_, front_end_msg| {
            if let Some(level) = rust_log_level_from_ipc(&front_end_msg) {
                if let Some(control) = log_level_control.as_ref() {
                    control.set_level(level);
                }
                return;
            }
            if front_end_msg == "showLogInExplorer" {
                tracing::debug!(
                    target: "msinsight_platform",
                    event = "open_log_directory_requested"
                );
                open_in_explorer(
                    cache_path
                        .as_ref()
                        .to_str()
                        .expect("Cache path is not valid UTF-8"),
                );
                return;
            }
            if front_end_msg.starts_with("openProjectInExplorer") {
                tracing::debug!(
                    target: "msinsight_platform",
                    event = "open_project_directory_requested"
                );
                handle_open_project_msg(&front_end_msg);
                return;
            }
            if front_end_msg.starts_with("openUrl") {
                tracing::debug!(
                    target: "msinsight_platform",
                    event = "open_external_url_requested"
                );
                handle_open_url_msg(&front_end_msg);
            }
        })
        .build()
}

fn handle_open_url_msg(front_end_msg: &str) {
    if let Some(index) = front_end_msg.find('|') {
        if index + 1 >= front_end_msg.len() {
            tracing::warn!(
                target: "msinsight_platform",
                event = "open_external_url_rejected",
                reason = "syntax_error"
            );
            return;
        }
        let url = &front_end_msg[index + 1..];
        open_in_explorer(url);
    }
}

fn handle_open_project_msg(front_end_msg: &str) {
    if let Some(index) = front_end_msg.find('|') {
        if index + 1 >= front_end_msg.len() {
            tracing::warn!(
                target: "msinsight_platform",
                event = "open_project_directory_rejected",
                reason = "syntax_error"
            );
            return;
        }
        let path = &front_end_msg[index + 1..];
        let mut path = Path::new(path);
        if !path.exists() {
            tracing::warn!(
                target: "msinsight_platform",
                event = "open_project_directory_rejected",
                reason = "path_unavailable"
            );
            return;
        }
        if path.is_file() {
            path = path.parent().expect("Failed to get parent directory");
        }
        let path = path.to_str().expect("Path is not valid UTF-8");
        open_in_explorer(path);
    }
}

fn open_in_explorer(path: &str) {
    match Command::new("xdg-open").arg(path).spawn() {
        Ok(_) => tracing::debug!(
            target: "msinsight_platform",
            event = "external_opener_spawned",
            operation = "open"
        ),
        Err(_) => tracing::warn!(
            target: "msinsight_platform",
            event = "external_opener_spawn_failed",
            operation = "open",
            reason = "spawn_failed"
        ),
    }
}

fn handle_user_event(webview: &WebView, path: PathBuf) {
    match webview.evaluate_script(&format!("window.handleDrop({:#?})", path)) {
        Ok(()) => tracing::debug!(
            target: "msinsight_platform",
            event = "file_drop_script_submitted"
        ),
        Err(_) => tracing::warn!(
            target: "msinsight_platform",
            event = "file_drop_script_submission_failed",
            reason = "submission_failed"
        ),
    }
}

pub fn run_event_loop(event_loop: EventLoop<PathBuf>, webview: WebView) {
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;
        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested,
                ..
            }
            | Event::WindowEvent {
                event: WindowEvent::Destroyed,
                ..
            } => {
                cleanup::handle_close_requested();
                *control_flow = ControlFlow::Exit;
            }
            Event::UserEvent(path) => handle_user_event(&webview, path),
            _ => (),
        }
    });
}

pub fn run_script(
    root_path: &PathBuf,
    cache_path: &PathBuf,
    port: u16,
    log_level_control: Option<crate::logging::LogLevelControl>,
) -> wry024::Result<(EventLoop<PathBuf>, WebView)> {
    let event_loop = EventLoop::with_user_event();

    let proxy = Arc::new(event_loop.create_proxy());

    let window = WindowBuilder::new()
        .with_title("MindStudio Insight")
        .with_maximized(true)
        .build(&event_loop)
        .expect("Error occurred when create App window");

    let resource_path = Arc::new(root_path.to_path_buf());
    let log_path = Arc::new(cache_path.to_path_buf());
    let webview = create_webview(
        window,
        log_path,
        resource_path,
        port,
        proxy,
        log_level_control,
    )?;

    Ok((event_loop, webview))
}

fn extract_mimetype(path: &str) -> &str {
    let mut mimetype = MIMETYPE_HTML;
    if let Some((_, ext)) = path.rsplit_once('.') {
        mimetype = match ext {
            "html" => "text/html",
            "js" => "text/javascript",
            "css" => "text/css",
            "svg" => "image/svg+xml",
            "png" => "image/png",
            "wasm" => "application/wasm",
            _ => MIMETYPE_HTML,
        }
    }

    mimetype
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::logging::RustLogLevel;

    #[test]
    fn resource_log_name_identifies_frontend_assets() {
        for (path, expected) in [
            ("/resources/profiler/frontend/index.html", "index.html"),
            (
                "/resources/profiler/frontend/static/js/main.7d331fa9.js",
                "main.7d331fa9.js",
            ),
            (
                "/resources/profiler/frontend/static/icons/gpu.svg",
                "gpu.svg",
            ),
        ] {
            assert_eq!(resource_name_for_log(path), Some(expected));
        }
    }

    #[test]
    fn resource_log_name_rejects_unrelated_or_malformed_paths() {
        for path in [
            "/Users/private/profile.json",
            "/resources/profiler/frontend-private/profile.json",
            "/resources/profiler/frontend/",
            "/resources/profiler/frontend/../private.json",
            "/resources/profiler/frontend/./private.json",
            "/resources/profiler/frontend/static\\private.json",
            "/resources/profiler/frontend/%2e%2e/private.json",
            "/resources/profiler/frontend/index.html?token=private",
            "/resources/profiler/frontend/private\n.js",
            "/resources/profiler/frontend/用户.json",
        ] {
            assert_eq!(resource_name_for_log(path), None, "{path:?}");
        }
    }

    #[test]
    fn rust_log_level_ipc_accepts_only_two_exact_messages() {
        assert_eq!(
            rust_log_level_from_ipc("setRustLogLevel|DEBUG"),
            Some(RustLogLevel::Debug)
        );
        assert_eq!(
            rust_log_level_from_ipc("setRustLogLevel|INFO"),
            Some(RustLogLevel::Info)
        );

        for message in [
            "setRustLogLevel|debug",
            "setRustLogLevel|WARN",
            "setRustLogLevel|ERROR",
            "setRustLogLevel|TRACE",
            "setRustLogLevel|OFF",
            "setRustLogLevel|",
            "setRustLogLevel|DEBUG|extra",
            " setRustLogLevel|DEBUG",
            "setRustLogLevel|INFO ",
            "PRIVATE_RAW_IPC_SENTINEL",
        ] {
            assert_eq!(rust_log_level_from_ipc(message), None, "{message:?}");
        }
    }
}
