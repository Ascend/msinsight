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
    resolve_decoded_resource_path(root, &decoded_path)
}

/// Only frontend static content is exposed through the WebView protocol.
/// `executable_root` is the parent of current_exe, not a URI-controlled root.
pub(crate) fn resolve_frontend_resource_path(
    executable_root: &Path,
    uri_path: &str,
) -> io::Result<PathBuf> {
    let decoded_path = decode_uri_path(uri_path)?;
    let suffix = decoded_path
        .strip_prefix("/resources/profiler/frontend/")
        .ok_or_else(invalid_resource_path)?;
    let canonical_executable_root = fs::canonicalize(executable_root)?;
    let frontend_root = frontend_resource_root(&canonical_executable_root)?;

    // The suffix is already decoded: a literal percent in a filename must not
    // be decoded a second time. Keep the generic component/containment checks.
    resolve_decoded_resource_path(&frontend_root, &format!("/{suffix}"))
}

fn frontend_resource_root(canonical_executable_root: &Path) -> io::Result<PathBuf> {
    let mut anchor = canonical_executable_root;
    let mut resources_name = "resources";

    #[cfg(target_os = "macos")]
    if canonical_executable_root.file_name() == Some(std::ffi::OsStr::new("MacOS")) {
        if let Some(contents) = canonical_executable_root.parent() {
            if contents.file_name() == Some(std::ffi::OsStr::new("Contents"))
                && contents.parent().and_then(Path::extension)
                    == Some(std::ffi::OsStr::new("app"))
            {
                anchor = contents;
                resources_name = "Resources";
            }
        }
    }

    // Never trust a canonicalized static root merely because it exists: each
    // fixed directory must stay at its expected location under the anchor.
    // This also rejects Resources/profiler/frontend redirected elsewhere in
    // the same installation. The MacOS/resources compatibility link is not
    // followed: only the standard sibling Contents/Resources is selected.
    let mut expected = anchor.to_path_buf();
    for directory in [resources_name, "profiler", "frontend"] {
        expected.push(directory);
        if fs::canonicalize(&expected)? != expected {
            return Err(io::Error::new(
                ErrorKind::PermissionDenied,
                "resource root escapes expected static location",
            ));
        }
    }
    Ok(expected)
}

fn resolve_decoded_resource_path(
    root: &Path,
    decoded_path: &str,
) -> io::Result<PathBuf> {
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

    fn write_fixture_file(path: &Path) {
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, b"static content").unwrap();
    }

    #[test]
    fn frontend_resolves_nonbundle_static_uris_and_agent_page() {
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        for name in ["index.html", "plugins/InsightWebAgent/index.html", "static/js/app.js"] {
            let resource = root.join("resources/profiler/frontend").join(name);
            write_fixture_file(&resource);
            let uri = format!("/resources/profiler/frontend/{name}");
            let expected = fs::canonicalize(resource).unwrap();
            assert_eq!(resolve_resource_path(&root, &uri).unwrap(), expected);
            assert_eq!(resolve_frontend_resource_path(&root, &uri).unwrap(), expected);
        }
    }

    #[test]
    fn frontend_decodes_safe_filenames_only_once() {
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        for (name, encoded) in [
            ("app bundle.js", "app%20bundle.js"),
            ("app%20bundle.js", "app%2520bundle.js"),
            ("%2e%2e", "%252e%252e"),
            ("literal%.js", "literal%25.js"),
        ] {
            let resource = root.join("resources/profiler/frontend").join(name);
            write_fixture_file(&resource);
            assert_eq!(
                resolve_frontend_resource_path(
                    &root, &format!("/resources/profiler/frontend/{encoded}"),
                ).unwrap(),
                fs::canonicalize(resource).unwrap(),
            );
        }
    }

    #[test]
    fn frontend_rejects_malformed_and_escaping_uris() {
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        write_fixture_file(&root.join("resources/profiler/frontend/index.html"));
        for path in [
            "/resources/profiler/frontend/",
            "/resources/profiler/frontend/../server/credentials.json",
            "/resources/profiler/frontend/%2e%2e/server/credentials.json",
            "/resources/profiler/frontend/static/%2E%2E/%2e%2e/config.json",
            "/resources/profiler/frontend/static\\private.json",
            "/resources/profiler/frontend/%5cprivate.json",
            "/resources/profiler/frontend//etc/passwd",
            "/resources/profiler/frontend/%2fetc/passwd",
            "/resources/profiler/frontend/app%",
            "/resources/profiler/frontend/app%2",
            "/resources/profiler/frontend/app%zz",
            "/resources/profiler/frontend/%ff",
            "/resources/profiler/frontend/%00",
            "resources/profiler/frontend/index.html",
            "//resources/profiler/frontend/index.html",
            "/resources/profiler/frontend-private/index.html",
        ] {
            assert!(resolve_frontend_resource_path(&root, path).is_err(), "{path:?}");
        }
    }

    #[test]
    fn frontend_does_not_expose_existing_installation_files() {
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        write_fixture_file(&root.join("resources/profiler/frontend/index.html"));
        for name in [
            "resources/profiler/server/credentials.json",
            "resources/profiler/server/profiler_server",
            "resources/profiler/server/insight_web_agent/config.json",
            "resources/profiler/credentials.json",
            "config/credentials.json",
            ".mindstudio_insight/cache.json",
            "MindStudioInsight",
        ] {
            write_fixture_file(&root.join(name));
            assert_eq!(
                resolve_frontend_resource_path(&root, &format!("/{name}"))
                    .unwrap_err().kind(),
                ErrorKind::InvalidInput,
            );
        }
    }

    #[cfg(unix)]
    #[test]
    fn frontend_rejects_file_symlinks_outside_static_root() {
        use std::os::unix::fs::symlink;
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        let frontend = root.join("resources/profiler/frontend");
        fs::create_dir_all(&frontend).unwrap();
        for (index, target) in [
            directory.path.join("outside.txt"),
            root.join("resources/profiler/server/credentials.json"),
            root.join("config/credentials.json"),
        ].iter().enumerate() {
            write_fixture_file(target);
            let name = format!("link-{index}.txt");
            symlink(target, frontend.join(&name)).unwrap();
            assert_eq!(
                resolve_frontend_resource_path(
                    &root, &format!("/resources/profiler/frontend/{name}"),
                ).unwrap_err().kind(),
                ErrorKind::PermissionDenied,
            );
        }
    }

    #[cfg(unix)]
    #[test]
    fn frontend_rejects_redirected_static_root_directories() {
        use std::os::unix::fs::symlink;
        for relative in ["resources", "resources/profiler", "resources/profiler/frontend"] {
            for internal in [false, true] {
                let directory = TestDirectory::new();
                let root = directory.path.join("installation");
                let target = if internal { root.join("private") }
                    else { directory.path.join("outside") };
                let redirected = root.join(relative);
                fs::create_dir_all(redirected.parent().unwrap()).unwrap();
                // Populate a valid-looking tree so rejection cannot be merely
                // a missing candidate error after trusting the symlink root.
                let remainder = "resources/profiler/frontend"
                    .strip_prefix(relative).unwrap().trim_start_matches('/');
                write_fixture_file(&target.join(remainder).join("index.html"));
                symlink(&target, redirected).unwrap();
                assert_eq!(
                    resolve_frontend_resource_path(
                        &root, "/resources/profiler/frontend/index.html",
                    ).unwrap_err().kind(),
                    ErrorKind::PermissionDenied,
                );
            }
        }
    }

    // Bundle selection is macOS-only, matching the production platform gate.
    // These filesystem fixtures do not launch Wry or validate a signed .app.
    #[cfg(target_os = "macos")]
    #[test]
    fn frontend_resolves_standard_bundle_with_or_without_compatibility_link() {
        use std::os::unix::fs::symlink;
        for with_link in [false, true] {
            let directory = TestDirectory::new();
            let contents = directory.path.join("MindStudioInsight.app/Contents");
            let root = contents.join("MacOS");
            let frontend = contents.join("Resources/profiler/frontend");
            fs::create_dir_all(&root).unwrap();
            for name in ["index.html", "plugins/InsightWebAgent/index.html", "static/js/app.js"] {
                write_fixture_file(&frontend.join(name));
            }
            if with_link {
                symlink("../Resources", root.join("resources")).unwrap();
                assert_eq!(
                    resolve_resource_path(&root, "/resources/profiler/frontend/index.html")
                        .unwrap_err().kind(),
                    ErrorKind::PermissionDenied,
                );
            }
            for name in ["index.html", "plugins/InsightWebAgent/index.html", "static/js/app.js"] {
                assert_eq!(
                    resolve_frontend_resource_path(
                        &root, &format!("/resources/profiler/frontend/{name}"),
                    ).unwrap(),
                    fs::canonicalize(frontend.join(name)).unwrap(),
                );
            }
            write_fixture_file(&contents.join("Resources/profiler/server/credentials.json"));
            write_fixture_file(&root.join("MindStudioInsight"));
            for path in ["/resources/profiler/server/credentials.json", "/MindStudioInsight"] {
                assert_eq!(
                    resolve_frontend_resource_path(&root, path).unwrap_err().kind(),
                    ErrorKind::InvalidInput,
                );
            }
        }
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn frontend_rejects_redirected_bundle_resources_and_frontend_roots() {
        use std::os::unix::fs::symlink;
        for relative in ["Resources", "Resources/profiler", "Resources/profiler/frontend"] {
            for internal in [false, true] {
                let directory = TestDirectory::new();
                let contents = directory.path.join("MindStudioInsight.app/Contents");
                let root = contents.join("MacOS");
                fs::create_dir_all(&root).unwrap();
                let target = if internal { contents.join("private") }
                    else { directory.path.join("outside") };
                let remainder = "Resources/profiler/frontend"
                    .strip_prefix(relative).unwrap().trim_start_matches('/');
                write_fixture_file(&target.join(remainder).join("index.html"));
                let redirected = contents.join(relative);
                fs::create_dir_all(redirected.parent().unwrap()).unwrap();
                symlink(&target, redirected).unwrap();
                symlink("../Resources", root.join("resources")).unwrap();
                assert_eq!(
                    resolve_frontend_resource_path(
                        &root, "/resources/profiler/frontend/index.html",
                    ).unwrap_err().kind(),
                    ErrorKind::PermissionDenied,
                );
            }
        }
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn frontend_rejects_bundle_file_symlink_into_server_or_outside_bundle() {
        use std::os::unix::fs::symlink;
        let directory = TestDirectory::new();
        let contents = directory.path.join("MindStudioInsight.app/Contents");
        let root = contents.join("MacOS");
        let frontend = contents.join("Resources/profiler/frontend");
        fs::create_dir_all(&root).unwrap();
        fs::create_dir_all(&frontend).unwrap();
        symlink("../Resources", root.join("resources")).unwrap();
        for (index, target) in [
            contents.join("Resources/profiler/server/credentials.json"),
            directory.path.join("outside.txt"),
            root.join("MindStudioInsight"),
        ].iter().enumerate() {
            write_fixture_file(target);
            let name = format!("link-{index}.txt");
            symlink(target, frontend.join(&name)).unwrap();
            assert_eq!(
                resolve_frontend_resource_path(
                    &root, &format!("/resources/profiler/frontend/{name}"),
                ).unwrap_err().kind(),
                ErrorKind::PermissionDenied,
            );
        }
    }

    #[cfg(target_os = "macos")]
    #[test]
    fn frontend_does_not_infer_bundle_from_macos_directory_name_alone() {
        let directory = TestDirectory::new();
        for relative in ["ordinary/MacOS", "not-an-app/Contents/MacOS"] {
            let root = directory.path.join(relative);
            let resource = root.join("resources/profiler/frontend/index.html");
            write_fixture_file(&resource);
            assert_eq!(
                resolve_frontend_resource_path(&root, "/resources/profiler/frontend/index.html").unwrap(),
                fs::canonicalize(resource).unwrap(),
            );
        }
    }

    #[cfg(windows)]
    #[test]
    fn frontend_rejects_windows_drive_path() {
        let directory = TestDirectory::new();
        let root = directory.path.join("installation");
        write_fixture_file(&root.join("resources/profiler/frontend/index.html"));
        assert_eq!(
            resolve_frontend_resource_path(&root, "/resources/profiler/frontend/C:/Windows/win.ini")
                .unwrap_err().kind(),
            ErrorKind::InvalidInput,
        );
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
