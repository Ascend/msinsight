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
pub(crate) mod cleanup;

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

fn create_webview(
    window: Window,
    cache_path: Arc<PathBuf>,
    resource_path: Arc<PathBuf>,
    port: u16,
    acp_port: u16,
    capability_token: &str,
    proxy: Arc<EventLoopProxy<PathBuf>>,
) -> wry024::Result<WebView> {
    WebViewBuilder::new(window)?
        .with_custom_protocol("wry".into(), move |request| {
            let path = request.uri().path();
            let content = match read(resource_path.join(&path[1..]).as_path()) {
                Ok(content) => content.into(),
                Err(e) => return Err(wry024::Error::Io(e)),
            };

            let mimetype = extract_mimetype(path);

            Response::builder()
                .header(CONTENT_TYPE, mimetype)
                .body(content)
                .map_err(Into::into)
        })
        .with_url(
            format!(
                "wry://localhost/resources/profiler/frontend/index.html?port={}&acpPort={}&acpCapabilityToken={}",
                port, acp_port, capability_token
            )
            .as_str(),
        )?
        .with_file_drop_handler(move |_, ev| {
            match ev {
                FileDropEvent::Dropped(paths) => {
                    if let Err(e) = proxy.send_event(paths[0].to_owned()) {
                        eprintln!("app closed unexpectedly: {:#?}", e);
                    }
                }
                _ => {}
            }

            true
        })
        .with_ipc_handler(move |_, front_end_msg| {
            println!("Platform received message from frontend: {}", front_end_msg);
            if front_end_msg == "showLogInExplorer" {
                open_in_explorer(
                    cache_path
                        .as_ref()
                        .to_str()
                        .expect("Cache path is not valid UTF-8"),
                );
                return;
            }
            if front_end_msg.starts_with("openProjectInExplorer") {
                handle_open_project_msg(&front_end_msg);
                return;
            }
            if front_end_msg.starts_with("openUrl") {
                handle_open_url_msg(&front_end_msg);
            }
        })
        .build()
}

fn handle_open_url_msg(front_end_msg: &str) {
    if let Some(index) = front_end_msg.find('|') {
        if index + 1 >= front_end_msg.len() {
            eprintln!("Front end message: open url has a syntax error");
            return;
        }
        let url = &front_end_msg[index + 1..];
        open_in_explorer(url);
    }
}

fn handle_open_project_msg(front_end_msg: &str) {
    if let Some(index) = front_end_msg.find('|') {
        if index + 1 >= front_end_msg.len() {
            eprintln!("Front end message: open project in explorer has a syntax error");
            return;
        }
        let path = &front_end_msg[index + 1..];
        let mut path = Path::new(path);
        if !path.exists() {
            eprintln!("Front end message: open project in explorer path does not exist");
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
    let result = match Command::new("xdg-open").arg(path).spawn() {
        Ok(_) => format!("Opened {} successfully", path),
        Err(e) => format!("Failed to open {}, error message: {}", path, e),
    };
    println!("{}", result);
}

fn handle_user_event(webview: &WebView, path: PathBuf) {
    if let Err(e) = webview.evaluate_script(&format!("window.handleDrop({:#?})", path)) {
        eprintln!("drop-file ipc failed: {:#?}", e);
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
    acp_port: u16,
    capability_token: &str,
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
    let webview = create_webview(window, log_path, resource_path, port, acp_port, capability_token, proxy)?;

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
