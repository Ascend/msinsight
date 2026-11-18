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

#[cfg(any(target_os = "macos", test))]
use std::fs;
use std::{
    io,
    path::{Path, PathBuf},
};

/// Resolve macOS resources physically; preserve the existing path spelling elsewhere.
pub(crate) fn resources_root(executable_dir: &Path) -> io::Result<PathBuf> {
    #[cfg(target_os = "macos")]
    {
        let executable_dir = fs::canonicalize(executable_dir)?;
        if executable_dir.file_name() == Some(std::ffi::OsStr::new("MacOS")) {
            if let Some(contents) = executable_dir.parent() {
                if contents.file_name() == Some(std::ffi::OsStr::new("Contents"))
                    && contents.parent().and_then(Path::extension)
                        == Some(std::ffi::OsStr::new("app"))
                {
                    return fs::canonicalize(contents.join("Resources"));
                }
            }
        }
        fs::canonicalize(executable_dir.join("resources"))
    }
    #[cfg(not(target_os = "macos"))]
    {
        // Windows canonicalization adds a verbatim prefix that affected Node.js
        // versions cannot resolve for the ACP CLI entry. Keep the pre-existing
        // Windows/Linux path construction, including symlink spelling.
        Ok(executable_dir.join("resources"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resolves_resources_next_to_unbundled_executable() {
        let root = std::env::temp_dir()
            .join(format!("insight-runtime-paths-{}", std::process::id()));
        fs::create_dir_all(root.join("resources")).unwrap();
        #[cfg(target_os = "macos")]
        let expected = fs::canonicalize(root.join("resources")).unwrap();
        #[cfg(not(target_os = "macos"))]
        let expected = root.join("resources");
        assert_eq!(resources_root(&root).unwrap(), expected);
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn resolves_app_resources_without_macos_compatibility_link() {
        let root = std::env::temp_dir()
            .join(format!("insight-mac-layout-{}", std::process::id()));
        let contents = root.join("目录 with spaces.app/Contents");
        fs::create_dir_all(contents.join("MacOS")).unwrap();
        fs::create_dir_all(contents.join("Resources")).unwrap();
        assert_eq!(
            resources_root(&contents.join("MacOS")).unwrap(),
            fs::canonicalize(contents.join("Resources")).unwrap()
        );
        assert!(!contents.join("MacOS/resources").exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(not(target_os = "macos"))]
    #[test]
    fn preserves_non_macos_path_spelling_without_filesystem_access() {
        #[cfg(windows)]
        let root = Path::new(r"C:\未安装 with spaces\parent\..\Insight");
        #[cfg(not(windows))]
        let root = Path::new("/未安装 with spaces/parent/../Insight");
        assert_eq!(resources_root(root).unwrap(), root.join("resources"));
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    #[test]
    fn preserves_non_macos_symlink_spelling() {
        let root = std::env::temp_dir()
            .join(format!("insight-resource-alias-{}", std::process::id()));
        let physical = root.join("physical");
        let alias = root.join("alias");
        fs::create_dir_all(physical.join("resources")).unwrap();
        std::os::unix::fs::symlink(&physical, &alias).unwrap();
        let actual = resources_root(&alias).unwrap();
        fs::remove_dir_all(root).unwrap();
        assert_eq!(actual, alias.join("resources"));
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn resolves_mac_executable_and_resource_symlinks_physically() {
        let root = std::env::temp_dir()
            .join(format!("insight-mac-resource-alias-{}", std::process::id()));
        let contents = root.join("目录 with spaces.app/Contents");
        let physical = root.join("physical-resources");
        let alias = root.join("launcher");
        fs::create_dir_all(contents.join("MacOS")).unwrap();
        fs::create_dir_all(&physical).unwrap();
        std::os::unix::fs::symlink(contents.join("MacOS"), &alias).unwrap();
        std::os::unix::fs::symlink(&physical, contents.join("Resources")).unwrap();
        let actual = resources_root(&alias).unwrap();
        let expected = fs::canonicalize(&physical).unwrap();
        fs::remove_dir_all(root).unwrap();
        assert_eq!(actual, expected);
    }

    #[cfg(windows)]
    #[test]
    #[ignore = "requires INSIGHT_TEST_NODE; run explicitly on a Windows validation agent"]
    fn windows_node_cli_accepts_resource_entry_path() {
        let node = std::env::var_os("INSIGHT_TEST_NODE")
            .expect("set INSIGHT_TEST_NODE to the exact Node.js executable under test");
        let root = std::env::temp_dir()
            .join(format!("insight-node-entry-{}", std::process::id()))
            .join("目录 with spaces");
        // The regression starts with an ordinary Windows path. Explicit
        // verbatim inputs and paths requiring extended-length syntax are not
        // normalized by resources_root.
        assert!(!root.as_os_str().to_string_lossy().starts_with(r"\\?\"));
        let relative = "profiler/server/insight_web_agent";
        fs::create_dir_all(root.join("resources").join(relative)).unwrap();
        let acp_path = resources_root(&root).unwrap().join(relative);
        let entry = acp_path.join("index.mjs");
        fs::write(
            &entry,
            "export {}; process.stdout.write('insight-node-entry-ready\\n');",
        )
        .unwrap();
        // Exercise Node's real CLI path resolution before its ESM loader,
        // rather than only testing path strings or pathToFileURL.
        let result = std::process::Command::new(node).arg(entry).output();
        fs::remove_dir_all(root.parent().unwrap()).unwrap();
        let output = result.expect("failed to launch the configured Node.js executable");
        assert!(
            output.status.success(),
            "Node CLI failed: {}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert_eq!(output.stdout, b"insight-node-entry-ready\n");
    }
}
