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

use std::{env, path::PathBuf};

use tao::{
    event::{Event, WindowEvent},
    event_loop::{ControlFlow, EventLoop, EventLoopBuilder, EventLoopProxy},
    window::{Window, WindowBuilder},
};
use wry::{FileDropEvent, WebView, WebViewBuilder};

fn create_webview(
    window: &Window,
    proxy: EventLoopProxy<PathBuf>,
) -> wry::Result<WebView> {
    // Wry only borrows Window in newer versions; run_event_loop owns it later.
    let builder = WebViewBuilder::new(&window)
        .with_url("http://localhost:5174?port=9000")
        .with_file_drop_handler(move |ev| {
            match ev {
                FileDropEvent::Dropped { paths, .. } => {
                    let _ = proxy.send_event(paths[0].to_owned());
                }
                _ => {}
            }

            true
        });
    builder.build()
}

fn handle_user_event(webview: &WebView, path: PathBuf) {
    let _ = webview.evaluate_script(&format!("window.handleDrop({:#?})", path));
}

// Keep Window alive because WebView only borrows its native handle.
fn run_event_loop(event_loop: EventLoop<PathBuf>, webview: WebView, window: Window) {
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;
        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested, ..
            } => {
                window.set_visible(false);
                *control_flow = ControlFlow::Exit;
            }
            Event::UserEvent(path) => handle_user_event(&webview, path),
            _ => (),
        }
    });
}

#[cfg(windows)]
fn set_windows_icon(window: &Window) {
    use tao::{platform::windows::IconExtWindows, window::Icon};

    let app_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let icon_path = app_dir.join("resources/images/icons/mindstudio.ico");
    window.set_window_icon(Icon::from_path(icon_path, None).ok());
}

// run script
pub fn main() {
    let event_loop = EventLoopBuilder::<PathBuf>::with_user_event().build();

    let proxy = event_loop.create_proxy();

    let window_builder: WindowBuilder = WindowBuilder::new()
        .with_title("MindStudio Insight")
        .with_maximized(true);

    // EventLoop dispatches window events but does not own the Window.
    let window = window_builder
        .build(&event_loop)
        .expect("Error occurred when create App window");

    #[cfg(windows)]
    set_windows_icon(&window);

    let webview = create_webview(&window, proxy).unwrap();

    run_event_loop(event_loop, webview, window)
}
