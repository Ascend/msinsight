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

#![windows_subsystem = "windows"]
#![allow(non_snake_case)]

#[cfg(all(not(debug_assertions), feature = "default"))]
mod default;
#[cfg(all(not(debug_assertions), feature = "default", any(target_os = "windows", target_os = "macos")))]
mod webview;
#[cfg(all(not(debug_assertions), feature = "default", target_os = "linux"))]
#[path = "webview_legacy.rs"]
mod webview;

#[cfg(all(debug_assertions, feature = "default", any(target_os = "windows", target_os = "macos")))]
mod dev;
#[cfg(all(debug_assertions, feature = "default", target_os = "linux"))]
#[path = "dev_legacy.rs"]
mod dev;

#[cfg(not(feature = "default"))]
mod http;

fn main() {
    #[cfg(target_os = "linux")]
    {
        if getuid() == 0 && !allow_root() {
            eprintln!(
                "WARNING: Running as root is discouraged.\
                 Use '--allow-root' to override this restriction."
            );
            return;
        }
    }

    #[cfg(feature = "default")]
    {
        #[cfg(debug_assertions)]
        dev::main();
        #[cfg(not(debug_assertions))]
        default::main();
    }

    #[cfg(not(feature = "default"))]
    http::main();
}

#[cfg(target_os = "linux")]
fn allow_root() -> bool {
    let args: Vec<String> = std::env::args().collect();
    args.contains(&String::from("--allow-root"))
}

/// # Safety
///
/// This function contains unsafe assembly code. The safety relies on:
/// - Correct syscall number for target architecture (102 for x86_64, 174 for aarch64)
/// - Proper register usage as per calling conventions
/// - Uses nomem/nostack options to ensure no implicit memory access
/// - Uses preserves_flags option to ensure no flag register changes
#[cfg(target_os = "linux")]
fn getuid() -> u32 {
    use std::arch::asm;

    let uid: u32;

    #[cfg(target_arch = "x86_64")]
    /// ### Source
    /// ```asm
    /// // unix/sysv/linux/x86_64/getuid.S
    /// ENTRY(__getuid)
    ///     movl $__NR_getuid, %eax
    ///     syscall
    ///     ret
    /// END(__getuid)
    /// ```
    unsafe {
        asm!(
            "syscall",
            in("rax") 102,
            lateout("rax") uid,
            options(nostack, nomem, preserves_flags)
        )
    }

    #[cfg(target_arch = "aarch64")]
    /// ### Source
    /// ```asm
    /// // unix/sysv/linux/aarch64/syscall.S
    /// ENTRY(getuid)
    ///     mov x8, __NR_getuid
    ///     svc 0
    ///     ret
    /// END(getuid)
    /// ```
    unsafe {
        asm!(
            "svc 0",
            in("x8") 174,
            lateout("x0") uid,
            options(nostack, nomem, preserves_flags)
        )
    }

    uid
}
