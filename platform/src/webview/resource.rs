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

use std::{
    fs,
    io::{self, ErrorKind},
    path::{Component, Path, PathBuf},
};

pub(crate) fn resolve_resource_path(
    root: &Path,
    uri_path: &str,
) -> io::Result<PathBuf> {
    let decoded_path = decode_uri_path(uri_path)?;
    let relative_path =
        decoded_path.strip_prefix('/').ok_or_else(invalid_resource_path)?;

    if relative_path.is_empty() || relative_path.contains('\\') {
        return Err(invalid_resource_path());
    }

    let canonical_root = fs::canonicalize(root)?;
    let mut candidate = canonical_root.clone();
    for component in Path::new(relative_path).components() {
        match component {
            Component::Normal(segment) => candidate.push(segment),
            _ => return Err(invalid_resource_path()),
        }
    }

    // 逐段过滤只挡住 ".." 这类文本穿越；符号链接是在 canonicalize 阶段才解析的，
    // 所以必须对规范化后的结果再做一次根目录归属校验。
    let canonical_candidate = fs::canonicalize(candidate)?;
    if !canonical_candidate.starts_with(&canonical_root) {
        return Err(io::Error::new(
            ErrorKind::PermissionDenied,
            "resource path escapes resource root",
        ));
    }

    Ok(canonical_candidate)
}

fn decode_uri_path(path: &str) -> io::Result<String> {
    let bytes = path.as_bytes();
    let mut decoded = Vec::with_capacity(bytes.len());
    let mut index = 0;

    while index < bytes.len() {
        if bytes[index] != b'%' {
            decoded.push(bytes[index]);
            index += 1;
            continue;
        }

        if index + 2 >= bytes.len() {
            return Err(invalid_resource_path());
        }

        let high = decode_hex_digit(bytes[index + 1])
            .ok_or_else(invalid_resource_path)?;
        let low = decode_hex_digit(bytes[index + 2])
            .ok_or_else(invalid_resource_path)?;
        decoded.push((high << 4) | low);
        index += 3;
    }

    String::from_utf8(decoded).map_err(|_| invalid_resource_path())
}

fn decode_hex_digit(value: u8) -> Option<u8> {
    match value {
        b'0'..=b'9' => Some(value - b'0'),
        b'a'..=b'f' => Some(value - b'a' + 10),
        b'A'..=b'F' => Some(value - b'A' + 10),
        _ => None,
    }
}

fn invalid_resource_path() -> io::Error {
    io::Error::new(ErrorKind::InvalidInput, "invalid resource path")
}

#[cfg(test)]
mod tests {
    use std::{
        env, process,
        sync::atomic::{AtomicU64, Ordering},
    };

    use super::*;

    static TEST_DIRECTORY_ID: AtomicU64 = AtomicU64::new(0);

    struct TestDirectory {
        path: PathBuf,
    }

    impl TestDirectory {
        fn new() -> Self {
            let id = TEST_DIRECTORY_ID.fetch_add(1, Ordering::Relaxed);
            let path = env::temp_dir().join(format!(
                "msinsight-resource-path-test-{}-{id}",
                process::id()
            ));
            fs::create_dir_all(&path).unwrap();
            Self { path }
        }
    }

    impl Drop for TestDirectory {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.path);
        }
    }

    #[test]
    fn resolves_existing_file_below_resource_root() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        let resource = root.join("assets").join("app.js");
        fs::create_dir_all(resource.parent().unwrap()).unwrap();
        fs::write(&resource, b"content").unwrap();

        let resolved = resolve_resource_path(&root, "/assets/app.js").unwrap();

        assert_eq!(resolved, fs::canonicalize(resource).unwrap());
    }

    #[test]
    fn resolves_percent_encoded_file_name() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        let resource = root.join("assets").join("app bundle.js");
        fs::create_dir_all(resource.parent().unwrap()).unwrap();
        fs::write(&resource, b"content").unwrap();

        let resolved =
            resolve_resource_path(&root, "/assets/app%20bundle.js").unwrap();

        assert_eq!(resolved, fs::canonicalize(resource).unwrap());
    }

    #[test]
    fn rejects_plain_and_encoded_parent_traversal() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        fs::create_dir_all(&root).unwrap();

        for path in [
            "/../outside.txt",
            "/%2e%2e/outside.txt",
            "/assets/%2E%2E/%2e%2e/outside.txt",
        ] {
            assert_eq!(
                resolve_resource_path(&root, path).unwrap_err().kind(),
                ErrorKind::InvalidInput
            );
        }
    }

    #[test]
    fn rejects_malformed_percent_encoding() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        fs::create_dir_all(&root).unwrap();

        for path in ["/asset%", "/asset%2", "/asset%zz"] {
            assert_eq!(
                resolve_resource_path(&root, path).unwrap_err().kind(),
                ErrorKind::InvalidInput
            );
        }
    }

    #[test]
    fn rejects_absolute_and_backslash_paths() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        fs::create_dir_all(&root).unwrap();

        for path in ["//server/share/file", "/%5c%5cserver%5cshare%5cfile"] {
            assert_eq!(
                resolve_resource_path(&root, path).unwrap_err().kind(),
                ErrorKind::InvalidInput
            );
        }
    }

    #[cfg(windows)]
    #[test]
    fn rejects_windows_drive_path() {
        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        fs::create_dir_all(&root).unwrap();

        assert_eq!(
            resolve_resource_path(&root, "/C:/Windows/win.ini")
                .unwrap_err()
                .kind(),
            ErrorKind::InvalidInput
        );
    }

    #[cfg(unix)]
    #[test]
    fn rejects_symlink_that_escapes_resource_root() {
        use std::os::unix::fs::symlink;

        let directory = TestDirectory::new();
        let root = directory.path.join("root");
        let outside = directory.path.join("outside.txt");
        fs::create_dir_all(&root).unwrap();
        fs::write(&outside, b"secret").unwrap();
        symlink(&outside, root.join("link.txt")).unwrap();

        assert_eq!(
            resolve_resource_path(&root, "/link.txt").unwrap_err().kind(),
            ErrorKind::PermissionDenied
        );
    }
}
