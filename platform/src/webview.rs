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

mod cleanup;
#[cfg(windows)]
pub mod webview2err;

use std::{fs::read, path::PathBuf, sync::Arc, process::Command};
use std::path::Path;
#[cfg(target_os = "macos")]
use wry::application::menu::{MenuBar, MenuItem};
pub use wry::webview::webview_version;
use wry::{
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
    proxy: Arc<EventLoopProxy<PathBuf>>,
) -> wry::Result<WebView> {
    WebViewBuilder::new(window)?
        .with_custom_protocol("wry".into(), move |request| {
            let path = request.uri().path();
            let content = match read(resource_path.join(&path[1..]).as_path()) {
                Ok(a) => a.into(),
                Err(e) => return Err(wry::Error::Io(e)),
            };

            let mimetype = extract_mimetype(path);

            Response::builder()
                .header(CONTENT_TYPE, mimetype)
                .body(content)
                .map_err(Into::into)
        })
        .with_url(format!("wry://localhost/resources/profiler/frontend/index.html?port={}&acpPort={}", port, acp_port).as_str())?
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
            // "showLogInExplorer"表示打开日志路径
            if front_end_msg == "showLogInExplorer" {
                open_in_explorer(cache_path.as_ref().to_str().expect("Cache path is not valid UTF-8"));
                return;
            }
            // "openProjectInExplorer|***"表示打开文件路径，***是文件的具体路径
            if front_end_msg.starts_with("openProjectInExplorer") {
                handle_open_project_msg(&front_end_msg);
                return;
            }
            // "openUrl|***"表示打开外部链接，***是具体的链接路径
            if front_end_msg.starts_with("openUrl") {
                handle_open_url_msg(&front_end_msg);
                return;
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
        // 如果传入的 path 是一个文件，打开文件所在目录而不是文件
        if path.is_file() {
            path = path.parent().expect("Failed to get parent directory");
        }
        let path = path.to_str().expect("Path is not valid UTF-8");
        open_in_explorer(&path);
    }
}

fn open_in_explorer(path: &str) {
    #[cfg(target_os = "windows")]
    let mut command = Command::new("explorer");
    #[cfg(target_os = "linux")]
    let mut command = Command::new("xdg-open");
    #[cfg(target_os = "macos")]
    let mut command = Command::new("open");

    let result = match command
        .arg(path)
        .spawn()
    {
        Ok(_) => format!("Opened {} successfully", path),
        Err(e) => format!("Failed to open {}, error message: {}", path, e),
    };
    println!("{}", result);
}

fn handle_user_event(webview: &WebView, path: PathBuf) {
    if let Err(e) =
        webview.evaluate_script(&format!("window.handleDrop({:#?})", path))
    {
        eprintln!("drop-file ipc failed: {:#?}", e);
    }
}

pub fn run_event_loop(event_loop: EventLoop<PathBuf>, webview: WebView) {
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;
        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested, ..
            }
            | Event::WindowEvent { event: WindowEvent::Destroyed, .. } => {
                cleanup::handle_close_requested();
                *control_flow = ControlFlow::Exit;
            }
            Event::UserEvent(path) => handle_user_event(&webview, path),
            _ => (),
        }
    });
}

#[cfg(windows)]
fn set_windows_icon(window: &Window, root_path: &PathBuf) {
    use wry::application::{platform::windows::IconExtWindows, window::Icon};

    window.set_window_icon(
        Icon::from_path(
            root_path
                .to_path_buf()
                .join("resources/images/icons/mindstudio.ico"),
            None,
        )
        .ok(),
    );
}

#[cfg(target_os = "macos")]
fn macos_menu() -> MenuBar {
    let mut menu = MenuBar::new();

    let mut window_menu = MenuBar::new();
    window_menu.add_native_item(MenuItem::Minimize);
    window_menu.add_native_item(MenuItem::Hide);
    window_menu.add_native_item(MenuItem::HideOthers);
    window_menu.add_native_item(MenuItem::Separator);
    window_menu.add_native_item(MenuItem::Services);
    window_menu.add_native_item(MenuItem::Separator);
    window_menu.add_native_item(MenuItem::CloseWindow);
    window_menu.add_native_item(MenuItem::Quit);

    menu.add_submenu("Window", true, window_menu);

    let mut edit_menu = MenuBar::new();
    edit_menu.add_native_item(MenuItem::Cut);
    edit_menu.add_native_item(MenuItem::Copy);
    edit_menu.add_native_item(MenuItem::Paste);
    edit_menu.add_native_item(MenuItem::SelectAll);

    menu.add_submenu("Edit", true, edit_menu);

    menu
}

// run script
pub fn run_script(
    root_path: &PathBuf,
    cache_path: &PathBuf,
    port: u16,
    acp_port: u16,
) -> wry::Result<(EventLoop<PathBuf>, WebView)> {
    let event_loop = EventLoop::with_user_event();

    let proxy = Arc::new(event_loop.create_proxy());

    let mut window_builder: WindowBuilder = WindowBuilder::new()
        .with_title("MindStudio Insight")
        .with_maximized(true);

    #[cfg(target_os = "macos")]
    {
        let menu = macos_menu();
        window_builder = window_builder.with_menu(menu);
    }

    let window = window_builder
        .build(&event_loop)
        .expect("Error occurred when create App window");

    let resource_path = Arc::new(root_path.to_path_buf());

    let log_path = Arc::new(cache_path.to_path_buf());

    #[cfg(windows)]
    set_windows_icon(&window, root_path);

    let webview = create_webview(window, log_path, resource_path, port, acp_port, proxy)?;

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
