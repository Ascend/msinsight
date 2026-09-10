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
use std::ffi::CString;
#[cfg(windows)]
use std::fs::read_to_string;

enum HWND__ {}
const MB_ICONERROR: u32 = 0x00000010;

extern "system" {
    /// Displays a modal dialog box that contains a system icon,
    /// a set of buttons, and a brief application-specific message,
    /// such as status or error information.
    /// The message box returns an integer value
    /// that indicates which button the user clicked.
    ///
    /// ### FFI Signature
    /// ```c++
    /// int MessageBoxA(
    ///   [in, optional] HWND   hWnd,
    ///   [in, optional] LPCSTR lpText,
    ///   [in, optional] LPCSTR lpCaption,
    ///   [in]           UINT   uType
    /// );
    /// ```
    #[allow(non_snake_case)]
    fn MessageBoxA(
        hWnd: *mut HWND__,
        lpText: *const i8,
        lpCaption: *const i8,
        uType: u32,
    ) -> i32;
}

const RUNTIME_URL_CONFIG_PATH: &str = "config/runtime_url_config";

#[cfg(windows)]
pub fn show_webview_err_message() {
    let mut runtime_url = String::new();
    match read_to_string(RUNTIME_URL_CONFIG_PATH) {
        Ok(content) => {
            if content.len() > 100 {
                return;
            }
            runtime_url = content
        }
        Err(e) => eprintln!("read webview2 runtime url config failed: {e}"),
    }

    let message = format!("Please install from {}", runtime_url);
    show_error_message("Missing Dependencies", &message);
}

#[cfg(windows)]
pub fn show_error_message(title: &str, message: &str) {
    let message = CString::new(message).expect("CString::new failed");
    let title = CString::new(title).expect("CString::new failed");
    /// # Safety
    ///
    /// This FFI call requires the following invariants to be upheld:
    ///
    /// ## Parameters Safety
    /// 1. `hWnd` - Null pointer is explicitly passed as we don't need a parent window
    /// 2. `lpText` -
    ///    - Pointer must remain valid for the duration of the call
    ///    - `CString` ensures proper null-terminated UTF-8 encoding
    /// 3. `lpCaption` -
    ///    - Same safety guarantees as `lpText`
    ///
    /// ## Memory Safety
    /// - Both `CString` instances will be dropped after this function call returns.
    unsafe {
        MessageBoxA(
            std::ptr::null_mut(),
            message.as_ptr(),
            title.as_ptr(),
            MB_ICONERROR,
        );
    }
}
