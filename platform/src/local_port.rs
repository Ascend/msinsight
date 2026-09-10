/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

use std::net::{Ipv4Addr, Ipv6Addr, SocketAddrV4, SocketAddrV6, TcpListener};

/// Inclusive local frontend-backend port range used by the desktop app.
pub const FRONTEND_BACKEND_PORT_START: u16 = 9000;
pub const FRONTEND_BACKEND_PORT_END: u16 = 9100;

pub const NO_AVAILABLE_PORT_DIALOG_TITLE: &str = "Startup Failed";

pub fn find_first_available_port(start: u16, end: u16) -> Option<u16> {
    // 探测系统是否支持 IPv6 loopback 绑定：若不支持则仅检测 IPv4，避免在禁用 IPv6 的环境失效
    let ipv6_capable =
        TcpListener::bind(SocketAddrV6::new(Ipv6Addr::LOCALHOST, 0, 0, 0))
            .is_ok();

    for port in start..=end {
        let v4_ok = TcpListener::bind(SocketAddrV4::new(
            Ipv4Addr::new(127, 0, 0, 1),
            port,
        ))
        .is_ok();
        // IPv6 不可用时跳过 v6 检测，等价于仅检测 IPv4，保证兼容性
        let v6_ok = if ipv6_capable {
            TcpListener::bind(SocketAddrV6::new(
                Ipv6Addr::LOCALHOST,
                port,
                0,
                0,
            ))
            .is_ok()
        } else {
            true
        };
        if v4_ok && v6_ok {
            return Some(port);
        }
    }

    None
}

/// A Linux dialog tool counts as shown once it starts.
/// Esc / window-close usually exits 1 and must not fall through.
#[cfg(any(test, target_os = "linux"))]
pub fn dialog_tool_was_shown(
    status: Result<std::process::ExitStatus, std::io::Error>,
) -> bool {
    status.is_ok()
}

pub fn no_available_port_stderr(start: u16, end: u16) -> String {
    format!("No available port between {start} and {end}")
}

pub fn no_available_port_dialog_message(start: u16, end: u16) -> String {
    let mut message = format!(
        "No available port between {start} and {end}.\n\n\
         MindStudio Insight needs a local TCP port in this range for \
         frontend-backend communication."
    );

    #[cfg(windows)]
    {
        message.push_str(&format!(
            " Windows excluded port ranges (often created by Hyper-V, \
             WinNAT, WSL, or Docker) can block the entire range.\n\n\
             Check excluded ports:\n\
             netsh interface ipv4 show excludedportrange protocol=tcp\n\n\
             Clear or adjust any excluded range that covers {start}-{end}, \
             then retry."
        ));
    }

    #[cfg(not(windows))]
    {
        message.push_str(&format!(
            " Another process may be using the range, or the OS may have \
             reserved it.\n\n\
             Free a port in {start}-{end} and retry."
        ));
    }

    message
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn default_scan_range_stays_9000_to_9100() {
        assert_eq!(FRONTEND_BACKEND_PORT_START, 9000);
        assert_eq!(FRONTEND_BACKEND_PORT_END, 9100);
    }

    #[test]
    fn stderr_message_keeps_existing_wording() {
        assert_eq!(
            no_available_port_stderr(9000, 9100),
            "No available port between 9000 and 9100"
        );
    }

    #[test]
    fn dialog_message_includes_port_range() {
        assert_eq!(NO_AVAILABLE_PORT_DIALOG_TITLE, "Startup Failed");
        let message = no_available_port_dialog_message(9000, 9100);
        assert!(message.contains("9000"));
        assert!(message.contains("9100"));
    }

    #[cfg(windows)]
    #[test]
    fn dialog_message_includes_netsh_hint_on_windows() {
        let message = no_available_port_dialog_message(9000, 9100);
        assert!(message.contains(
            "netsh interface ipv4 show excludedportrange protocol=tcp"
        ));
    }

    #[cfg(not(windows))]
    #[test]
    fn dialog_message_asks_to_free_the_range_on_unix() {
        let message = no_available_port_dialog_message(9000, 9100);
        assert!(message.contains("Free a port in 9000-9100"));
        assert!(!message.contains("netsh"));
    }

    #[test]
    fn missing_dialog_binary_is_not_shown() {
        let status = std::process::Command::new(
            "__msinsight_missing_dialog_tool__",
        )
        .status();
        assert!(!dialog_tool_was_shown(status));
    }

    #[cfg(unix)]
    #[test]
    fn nonzero_dialog_exit_still_counts_as_shown() {
        let status = std::process::Command::new("false").status();
        assert!(dialog_tool_was_shown(status));
    }

    #[cfg(unix)]
    #[test]
    fn zero_dialog_exit_counts_as_shown() {
        let status = std::process::Command::new("true").status();
        assert!(dialog_tool_was_shown(status));
    }

    #[test]
    fn inverted_range_returns_none() {
        assert_eq!(find_first_available_port(9100, 9000), None);
    }

    #[test]
    fn returns_none_when_the_only_candidate_port_is_busy() {
        let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 0))
            .expect("bind an ephemeral IPv4 loopback port");
        let port = listener.local_addr().expect("read bound port").port();
        assert_eq!(find_first_available_port(port, port), None);
    }

    #[test]
    fn finds_an_open_port_inside_a_small_range() {
        let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 0))
            .expect("reserve an ephemeral IPv4 loopback port");
        let port = listener.local_addr().expect("read bound port").port();
        drop(listener);

        let found = find_first_available_port(port, port);
        assert_eq!(found, Some(port));
    }
}
