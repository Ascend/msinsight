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

use std::{env, env::current_exe, process::Command};

pub fn main() {
    use std::process::exit;

    let executable = current_exe().unwrap_or_else(|error| {
        eprintln!("Failed to determine the application executable path: {error}");
        exit(1);
    });
    let Some(executable_dir) = executable.parent() else {
        eprintln!(
            "The application executable has no parent directory: {}",
            executable.display()
        );
        exit(1);
    };
    let resources = crate::runtime_paths::resources_root(executable_dir).unwrap_or_else(|error| {
        eprintln!(
            "Failed to locate application resources from {}: {error}",
            executable_dir.display()
        );
        exit(1);
    });
    let profiler_path = resources.join("profiler").join("start_script.py");
    let mut server_command = Command::new("python3");
    server_command.arg(&profiler_path).args(env::args_os());
    match server_command.output() {
        Ok(output) => {
            eprint!("{}", String::from_utf8_lossy(&output.stdout));
            eprint!("{}", String::from_utf8_lossy(&output.stderr));
            if !output.status.success() {
                eprintln!(
                    "Server script {} failed with {}",
                    profiler_path.display(),
                    output.status
                );
            }
            exit(output.status.code().unwrap_or(1));
        }
        Err(error) => {
            eprintln!(
                "Failed to run or capture the python3 server process for {}: {error}",
                profiler_path.display()
            );
            exit(1);
        }
    };
}
