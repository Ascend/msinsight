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

#[cfg(any(target_os = "macos", target_os = "linux"))]
use std::collections::{HashMap, VecDeque};
#[cfg(windows)]
use std::os::windows::process::CommandExt;
#[cfg(any(target_os = "macos", target_os = "linux"))]
use std::process::ChildStdout;
use std::{
    io::{BufRead, BufReader, Result},
    process::{Command, Stdio},
};

use crate::default::{ACP_PID, PID};

#[cfg(windows)]
fn query_child_pids(parent_pid: u32) -> Result<Vec<String>> {
    let wmic_output = Command::new("wmic")
        .arg("process")
        .arg("where")
        .arg(format!("ParentProcessId={}", parent_pid))
        .arg("get")
        .arg("ProcessId")
        .creation_flags(0x08000000)
        .stdout(Stdio::piped())
        .spawn();

    let mut child_pids = Vec::new();
    match wmic_output {
        Ok(child) => {
            if let Some(stdout) = child.stdout {
                BufReader::new(stdout)
                    .lines()
                    .skip(1)
                    .filter_map(|line| line.ok())
                    .map(|line| line.trim().to_string())
                    .filter(|s| !s.is_empty())
                    .for_each(|pid| child_pids.push(pid));
            }
        }
        Err(e) => eprintln!("wmic command failed: {e}"),
    }
    Ok(child_pids)
}

#[cfg(any(target_os = "macos", target_os = "linux"))]
fn process_child_stdout_line(
    line: String,
    pid_map: &mut HashMap<String, Vec<String>>,
) {
    let parts: Vec<&str> = line.split_whitespace().collect();
    if let [key, value] = parts.as_slice() {
        pid_map
            .entry(key.to_string())
            .and_modify(|v| v.push(value.to_string()))
            .or_insert(vec![value.to_string()]);
    }
}

#[cfg(any(target_os = "macos", target_os = "linux"))]
fn process_child_stdout(
    stdout: ChildStdout,
    pid_map: &mut HashMap<String, Vec<String>>,
) {
    let reader = BufReader::new(stdout);
    for line in reader.lines().skip(1) {
        if let Ok(line) = line {
            process_child_stdout_line(line, pid_map)
        }
    }
}

#[cfg(any(target_os = "macos", target_os = "linux"))]
fn query_child_pids(parent_pid: u32) -> Result<Vec<String>> {
    let ps_output = Command::new("ps")
        .arg("-o")
        .arg("ppid,pid")
        .stdout(Stdio::piped())
        .spawn();
    let mut child_pids = Vec::new();
    let mut pid_map: HashMap<String, Vec<String>> = HashMap::new();
    match ps_output {
        Ok(pairs) => {
            if let Some(stdout) = pairs.stdout {
                process_child_stdout(stdout, &mut pid_map)
            }
        }
        Err(e) => eprintln!("ps command failed: {e}"),
    }

    let mut deque = VecDeque::new();
    deque.push_back(parent_pid.to_string());
    while let Some(cur) = deque.pop_back() {
        if let Some(children) = pid_map.get(&cur) {
            for item in children {
                child_pids.push(item.to_string());
                deque.push_back(item.to_string());
            }
        }
    }

    Ok(child_pids)
}

fn kill_process_tree(parent_pid: String) -> Result<()> {
    #[cfg(windows)]
    {
        // Windows 下使用 taskkill 命令
        Command::new("taskkill")
            .arg("/f")
            .arg("/t") // 终止包括子进程在内的所有进程
            .arg("/pid")
            .arg(parent_pid)
            .creation_flags(0x08000000)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .output()
            .expect("Failed to execute taskkill command");
    }
    #[cfg(any(target_os = "macos", target_os = "linux"))]
    {
        Command::new("kill")
            .arg("-9")
            .arg(parent_pid)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .expect("Failed to execute kill command");
    }

    Ok(())
}

fn kill_child_pids(child_pids: Vec<String>) {
    for child_pid in child_pids {
        if let Err(e) = kill_process_tree(child_pid) {
            eprintln!("Err when kill process: {e}");
        }
    }
}

pub fn handle_close_requested() {
    unsafe {
        for pid in [ACP_PID, PID] {
            if pid == u32::MAX {
                continue;
            }
            match query_child_pids(pid) {
                Ok(child_pids) => {
                    kill_child_pids(child_pids);
                    let _ = kill_process_tree(pid.to_string());
                }
                Err(e) => eprintln!("Err when query child pids {e}"),
            }
        }
    }
}
