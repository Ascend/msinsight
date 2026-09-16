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
    ffi::OsString,
    fs::OpenOptions,
    path::Path,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};

use tracing::{Level, Subscriber};
use tracing_subscriber::{
    filter::{dynamic_filter_fn, LevelFilter},
    layer::{Layer as _, SubscriberExt},
};

const LOG_TARGET: &str = "msinsight_platform";
const LOG_FILE_NAME: &str = "msinsight.log";
const CLI_PREFIX: &str = "--rust-log-level=";

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum LoggingInitError {
    OpenLogFile,
    InstallSubscriber,
}

impl std::fmt::Display for LoggingInitError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(match self {
            Self::OpenLogFile => "could not open Rust log file",
            Self::InstallSubscriber => "could not install Rust log subscriber",
        })
    }
}

impl std::error::Error for LoggingInitError {}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
// Rust 日志启动级别仅保留可公开配置的 DEBUG 和 INFO。
pub(crate) enum RustLogLevel {
    Debug,
    Info,
}

impl RustLogLevel {
    fn as_str(self) -> &'static str {
        match self {
            Self::Debug => "DEBUG",
            Self::Info => "INFO",
        }
    }
}

// Arc 与原子状态只属于一个已初始化的 Rust 进程控制实例；后续 WebView 持有其克隆，subscriber 则按事件读取同一状态。
#[derive(Clone)]
pub(crate) struct LogLevelControl {
    debug_enabled: Arc<AtomicBool>,
}

impl LogLevelControl {
    fn new(level: RustLogLevel) -> Self {
        Self {
            debug_enabled: Arc::new(AtomicBool::new(
                level == RustLogLevel::Debug,
            )),
        }
    }

    // 此切换不改变 C++ 日志，也不提供轮转、远程控制、进程发现或通用配置语言。
    pub(crate) fn set_level(&self, level: RustLogLevel) -> bool {
        let debug_enabled = level == RustLogLevel::Debug;
        if self.debug_enabled.swap(debug_enabled, Ordering::AcqRel)
            == debug_enabled
        {
            return false;
        }

        tracing::info!(
            target: LOG_TARGET,
            event = "rust_log_level_changed",
            level = level.as_str()
        );
        true
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
// 记录初始日志级别来自命令行还是默认值。
enum LevelSource {
    CommandLine,
    Default,
}

impl LevelSource {
    fn as_str(self) -> &'static str {
        match self {
            Self::CommandLine => "command_line",
            Self::Default => "default",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
// 启动配置只描述当前进程的初始状态，不承担运行时或跨进程控制。
struct StartupLogConfig {
    level: RustLogLevel,
    source: LevelSource,
    selected_value_was_invalid: bool,
}

fn parse_level(value: &str) -> Option<RustLogLevel> {
    match value {
        "DEBUG" => Some(RustLogLevel::Debug),
        "INFO" => Some(RustLogLevel::Info),
        _ => None,
    }
}

fn selected_config(
    value: Option<&str>,
    source: LevelSource,
) -> StartupLogConfig {
    match value.and_then(parse_level) {
        Some(level) => StartupLogConfig {
            level,
            source,
            selected_value_was_invalid: false,
        },
        None => StartupLogConfig {
            level: RustLogLevel::Info,
            source,
            selected_value_was_invalid: true,
        },
    }
}

// 仅解析等号形式的启动参数；其作用仅限于选择当前进程的初始级别。
fn resolve_startup_config(args: &[OsString]) -> StartupLogConfig {
    // 先按 ASCII 前缀匹配，再解码完整参数；这样非 Unicode 后缀会被选中并安全判定为非法值。
    if let Some(argument) = args.iter().skip(1).find(|argument| {
        argument.as_encoded_bytes().starts_with(CLI_PREFIX.as_bytes())
    }) {
        let value = argument
            .to_str()
            .and_then(|argument| argument.strip_prefix(CLI_PREFIX));
        return selected_config(value, LevelSource::CommandLine);
    }

    StartupLogConfig {
        level: RustLogLevel::Info,
        source: LevelSource::Default,
        selected_value_was_invalid: false,
    }
}

// 启动参数只选择初始状态，不提供运行时或跨进程日志控制。
fn process_startup_config() -> StartupLogConfig {
    let args: Vec<_> = std::env::args_os().collect();
    resolve_startup_config(&args)
}

fn allows(debug_enabled: bool, event_level: &Level) -> bool {
    matches!(*event_level, Level::INFO | Level::WARN | Level::ERROR)
        || debug_enabled && *event_level == Level::DEBUG
}

fn build_subscriber(
    cache_path: &Path,
    control: LogLevelControl,
) -> Result<impl Subscriber + Send + Sync, LoggingInitError> {
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(cache_path.join(LOG_FILE_NAME))
        .map_err(|_| LoggingInitError::OpenLogFile)?;
    let filter = dynamic_filter_fn(move |metadata, _context| {
        metadata.target() == LOG_TARGET
            && allows(
                control.debug_enabled.load(Ordering::Acquire),
                metadata.level(),
            )
    })
    .with_max_level_hint(LevelFilter::DEBUG);

    Ok(tracing_subscriber::registry().with(
        tracing_subscriber::fmt::layer()
            .with_writer(file)
            .with_ansi(false)
            .log_internal_errors(false)
            .with_filter(filter),
    ))
}

pub(crate) fn initialize(
    cache_path: &Path,
) -> Result<LogLevelControl, LoggingInitError> {
    let config = process_startup_config();
    let control = LogLevelControl::new(config.level);
    let subscriber = build_subscriber(cache_path, control.clone())?;
    tracing::subscriber::set_global_default(subscriber)
        .map_err(|_| LoggingInitError::InstallSubscriber)?;

    if config.selected_value_was_invalid {
        tracing::warn!(
            target: LOG_TARGET,
            event = "rust_log_level_config_invalid",
            source = config.source.as_str(),
            reason = "invalid_value"
        );
    }
    tracing::info!(
        target: LOG_TARGET,
        event = "logging_initialized",
        level = config.level.as_str(),
        source = config.source.as_str()
    );

    Ok(control)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        error::Error as _,
        ffi::OsStr,
        fs,
        path::{Path, PathBuf},
        process::Command,
        sync::atomic::{AtomicU64, Ordering},
    };
    use tracing::Dispatch;

    static NEXT_TEST_DIRECTORY: AtomicU64 = AtomicU64::new(0);
    const CHILD_COMPLETED: &str = "MSINSIGHT_LOGGING_CHILD_COMPLETED";

    struct TestDirectory {
        path: PathBuf,
    }

    impl TestDirectory {
        fn new(label: &str) -> Self {
            let sequence = NEXT_TEST_DIRECTORY.fetch_add(1, Ordering::Relaxed);
            let path = std::env::temp_dir().join(format!(
                "msinsight_logging_{label}_{}_{}",
                std::process::id(),
                sequence
            ));
            fs::create_dir(&path)
                .expect("create unique logging test directory");
            Self { path }
        }

        fn path(&self) -> &Path {
            &self.path
        }

        fn log_contents(&self) -> String {
            fs::read_to_string(self.path.join(LOG_FILE_NAME))
                .expect("read logging test output")
        }
    }

    impl Drop for TestDirectory {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.path);
        }
    }

    fn emit_with_local_subscriber(
        directory: &Path,
        level: RustLogLevel,
        emit: impl FnOnce(),
    ) {
        let subscriber =
            build_subscriber(directory, LogLevelControl::new(level))
                .expect("build local logging subscriber");
        let dispatch = Dispatch::new(subscriber);
        tracing::dispatcher::with_default(&dispatch, emit);
        drop(dispatch);
    }

    fn emit_level_probe(stage: &'static str) {
        tracing::trace!(
            target: LOG_TARGET,
            event = "filter_probe",
            stage,
            level = "TRACE"
        );
        tracing::debug!(
            target: LOG_TARGET,
            event = "filter_probe",
            stage,
            level = "DEBUG"
        );
        tracing::info!(
            target: LOG_TARGET,
            event = "filter_probe",
            stage,
            level = "INFO"
        );
        tracing::warn!(
            target: LOG_TARGET,
            event = "filter_probe",
            stage,
            level = "WARN"
        );
        tracing::error!(
            target: LOG_TARGET,
            event = "filter_probe",
            stage,
            level = "ERROR"
        );
    }

    fn has_exact_test_marker(value: Option<&OsStr>) -> bool {
        value == Some(OsStr::new("1"))
    }

    fn run_isolated_test(
        test_name: &str,
        child_environment: &str,
        environment: &[(&str, &str)],
    ) -> std::process::Output {
        let output = Command::new(std::env::current_exe().unwrap())
            .args([test_name, "--exact", "--test-threads=1", "--nocapture"])
            .env(child_environment, "1")
            .envs(environment.iter().copied())
            .output()
            .expect("run isolated logging test");
        let stdout = String::from_utf8_lossy(&output.stdout);
        assert!(
            output.status.success()
                && stdout.lines().any(|line| line == CHILD_COMPLETED),
            "child test did not complete: {test_name} ({})\nstdout:\n{}\nstderr:\n{}",
            output.status,
            stdout,
            String::from_utf8_lossy(&output.stderr)
        );
        output
    }

    fn assert_level_probe(output: &str, stage: &str, debug_expected: bool) {
        for (level, expected) in [
            ("TRACE", false),
            ("DEBUG", debug_expected),
            ("INFO", true),
            ("WARN", true),
            ("ERROR", true),
        ] {
            let present = output.lines().any(|line| {
                line.contains("event=\"filter_probe\"")
                    && line.contains(&format!("stage=\"{stage}\""))
                    && line.contains(&format!("level=\"{level}\""))
            });
            assert_eq!(present, expected, "{stage}: {level}");
        }
    }

    fn args(values: &[&str]) -> Vec<OsString> {
        values.iter().map(OsString::from).collect()
    }

    fn non_unicode_value(prefix: &str) -> OsString {
        #[cfg(windows)]
        {
            use std::os::windows::ffi::OsStringExt;

            let mut wide: Vec<u16> = prefix.encode_utf16().collect();
            wide.push(0xd800);
            OsString::from_wide(&wide)
        }

        #[cfg(unix)]
        {
            use std::os::unix::ffi::OsStringExt;

            let mut bytes = prefix.as_bytes().to_vec();
            bytes.push(0xff);
            OsString::from_vec(bytes)
        }
    }

    #[test]
    fn startup_accepts_only_info_and_debug() {
        for (argument, expected) in [
            ("--rust-log-level=INFO", RustLogLevel::Info),
            ("--rust-log-level=DEBUG", RustLogLevel::Debug),
        ] {
            let config = resolve_startup_config(&args(&["app", argument]));
            assert_eq!(config.level, expected);
            assert_eq!(config.source, LevelSource::CommandLine);
            assert!(!config.selected_value_was_invalid);
        }
    }

    #[test]
    fn unsupported_startup_values_fall_back_to_info() {
        for value in [
            "WARN",
            "ERROR",
            "TRACE",
            "OFF",
            "",
            "debug",
            "INFO,third_party=TRACE",
            " INFO ",
        ] {
            let argument = format!("{CLI_PREFIX}{value}");
            let config = resolve_startup_config(&args(&["app", &argument]));
            assert_eq!(config.level, RustLogLevel::Info, "{value:?}");
            assert_eq!(config.source, LevelSource::CommandLine, "{value:?}");
            assert!(config.selected_value_was_invalid, "{value:?}");
        }
    }

    #[test]
    fn separated_startup_form_is_ignored() {
        let config = resolve_startup_config(&args(&[
            "app",
            "--rust-log-level",
            "DEBUG",
        ]));
        assert_eq!(config.level, RustLogLevel::Info);
        assert_eq!(config.source, LevelSource::Default);
        assert!(!config.selected_value_was_invalid);
    }

    #[test]
    fn first_equals_form_cli_option_is_selected() {
        let cases: &[(&[&str], RustLogLevel, bool)] = &[
            (
                &["app", "--rust-log-level=DEBUG", "--rust-log-level=INFO"],
                RustLogLevel::Debug,
                false,
            ),
            (
                &["app", "--rust-log-level=TRACE", "--rust-log-level=DEBUG"],
                RustLogLevel::Info,
                true,
            ),
            (
                &["app", "--rust-log-level=", "--rust-log-level=DEBUG"],
                RustLogLevel::Info,
                true,
            ),
        ];

        for &(values, expected_level, expected_invalid) in cases {
            let config = resolve_startup_config(&args(values));
            assert_eq!(config.level, expected_level, "{values:?}");
            assert_eq!(config.source, LevelSource::CommandLine, "{values:?}");
            assert_eq!(
                config.selected_value_was_invalid, expected_invalid,
                "{values:?}"
            );
        }
    }

    #[test]
    fn invalid_value_is_not_retained() {
        let sentinel = "PRIVATE_RAW_LEVEL";
        let config = resolve_startup_config(&args(&[
            "app",
            &format!("{CLI_PREFIX}{sentinel}"),
        ]));
        assert!(config.selected_value_was_invalid);
        assert!(!format!("{config:?}").contains(sentinel));
    }

    #[test]
    fn unrelated_non_unicode_argv_is_ignored() {
        let mut values = args(&["app"]);
        values.push(non_unicode_value("unrelated="));
        let config = resolve_startup_config(&values);
        assert_eq!(config.level, RustLogLevel::Info);
        assert_eq!(config.source, LevelSource::Default);
        assert!(!config.selected_value_was_invalid);
    }

    #[test]
    fn non_unicode_selected_cli_is_invalid_without_panic() {
        let mut values = args(&["app"]);
        values.push(non_unicode_value(CLI_PREFIX));
        let config = resolve_startup_config(&values);
        assert_eq!(config.level, RustLogLevel::Info);
        assert_eq!(config.source, LevelSource::CommandLine);
        assert!(config.selected_value_was_invalid);
    }

    #[test]
    #[should_panic(expected = "child test did not complete")]
    fn isolated_test_rejects_missing_test() {
        run_isolated_test(
            "logging::tests::missing_child_test",
            "MSINSIGHT_TEST_MISSING_CHILD",
            &[],
        );
    }

    #[test]
    fn double_install_child_mode_requires_exact_marker() {
        assert!(!has_exact_test_marker(None));
        assert!(!has_exact_test_marker(Some(OsStr::new(""))));
        assert!(!has_exact_test_marker(Some(OsStr::new("0"))));
        assert!(has_exact_test_marker(Some(OsStr::new("1"))));
    }

    #[test]
    fn startup_ignores_rust_log_environment() {
        const CHILD_ENVIRONMENT: &str = "MSINSIGHT_TEST_RUST_LOG_IGNORED";
        const TEST_NAME: &str =
            concat!("logging::tests::", "startup_ignores_rust_log_environment");

        if has_exact_test_marker(std::env::var_os(CHILD_ENVIRONMENT).as_deref())
        {
            let directory = TestDirectory::new("rust_log_ignored");
            initialize(directory.path()).expect("initialize child logging");
            tracing::debug!(
                target: LOG_TARGET,
                "rust_log_ignored_debug_marker"
            );
            tracing::info!(
                target: LOG_TARGET,
                "rust_log_ignored_info_marker"
            );

            let output = directory.log_contents();
            assert!(!output.contains("rust_log_ignored_debug_marker"));
            assert!(output.contains("rust_log_ignored_info_marker"));
            assert!(output.contains("event=\"logging_initialized\""));
            assert!(output.contains("source=\"default\""));
            println!("\n{CHILD_COMPLETED}");
            return;
        }

        run_isolated_test(
            TEST_NAME,
            CHILD_ENVIRONMENT,
            &[("RUST_LOG", "DEBUG")],
        );
    }

    #[test]
    fn writer_creates_and_appends_formatted_events_to_fixed_file() {
        let directory = TestDirectory::new("append");
        let log_path = directory.path().join("msinsight.log");
        let mut previous = String::new();
        for marker in ["first_append_marker", "second_append_marker"] {
            emit_with_local_subscriber(
                directory.path(),
                RustLogLevel::Info,
                || tracing::info!(target: "msinsight_platform", message = marker),
            );
            let output = fs::read_to_string(&log_path)
                .expect("read fixed log file after closing each subscriber");
            assert!(output.as_bytes().starts_with(previous.as_bytes()));
            assert!(output.len() > previous.len());
            let appended = &output[previous.len()..];
            let timestamp = appended
                .split_whitespace()
                .next()
                .expect("formatted event has timestamp");
            assert!(timestamp.contains('T'));
            assert!(appended.contains("INFO"));
            assert!(appended.contains("msinsight_platform"));
            assert!(appended.contains(marker));
            assert!(!appended.as_bytes().contains(&0x1b));
            previous = output;
        }
    }

    #[test]
    fn runtime_filter_switches_info_debug_info_without_losing_higher_events() {
        let directory = TestDirectory::new("runtime_switch");
        let control = LogLevelControl::new(RustLogLevel::Info);
        let subscriber = build_subscriber(directory.path(), control.clone())
            .expect("build dynamic logging subscriber");
        let dispatch = Dispatch::new(subscriber);

        tracing::dispatcher::with_default(&dispatch, || {
            emit_level_probe("before");
            assert!(!control.set_level(RustLogLevel::Info));
            assert!(control.set_level(RustLogLevel::Debug));
            emit_level_probe("during");
            assert!(!control.set_level(RustLogLevel::Debug));
            assert!(control.set_level(RustLogLevel::Info));
            assert!(!control.set_level(RustLogLevel::Info));
            emit_level_probe("after");
        });
        drop(dispatch);

        let output = directory.log_contents();
        for (stage, debug_expected) in
            [("before", false), ("during", true), ("after", false)]
        {
            assert_level_probe(&output, stage, debug_expected);
        }

        let changed_events: Vec<_> = output
            .lines()
            .filter(|line| line.contains("event=\"rust_log_level_changed\""))
            .collect();
        assert_eq!(changed_events.len(), 2);
        assert!(changed_events[0].contains("level=\"DEBUG\""));
        assert!(changed_events[1].contains("level=\"INFO\""));
    }

    #[test]
    fn independently_constructed_controls_do_not_share_state() {
        let debug_directory = TestDirectory::new("independent_debug");
        let info_directory = TestDirectory::new("independent_info");
        let debug_control = LogLevelControl::new(RustLogLevel::Info);
        let info_control = LogLevelControl::new(RustLogLevel::Info);
        let debug_dispatch = Dispatch::new(
            build_subscriber(debug_directory.path(), debug_control.clone())
                .expect("build first logging subscriber"),
        );
        let info_dispatch = Dispatch::new(
            build_subscriber(info_directory.path(), info_control)
                .expect("build second logging subscriber"),
        );

        tracing::dispatcher::with_default(&debug_dispatch, || {
            assert!(debug_control.set_level(RustLogLevel::Debug));
            emit_level_probe("independent");
        });
        tracing::dispatcher::with_default(&info_dispatch, || {
            emit_level_probe("independent");
        });
        drop(debug_dispatch);
        drop(info_dispatch);

        assert_level_probe(
            &debug_directory.log_contents(),
            "independent",
            true,
        );
        assert_level_probe(
            &info_directory.log_contents(),
            "independent",
            false,
        );
    }

    #[test]
    fn initial_filter_matches_info_and_debug_thresholds() {
        for (level, debug_expected) in
            [(RustLogLevel::Debug, true), (RustLogLevel::Info, false)]
        {
            let directory = TestDirectory::new(level.as_str());
            emit_with_local_subscriber(directory.path(), level, || {
                emit_level_probe("initial");
            });
            assert_level_probe(
                &directory.log_contents(),
                "initial",
                debug_expected,
            );
        }
    }

    #[test]
    fn exact_target_filter_rejects_prefix_child_and_third_party_targets() {
        const REJECTED_SENTINEL: &str = "PRIVATE_REJECTED_TARGET_SENTINEL";
        for level in [RustLogLevel::Info, RustLogLevel::Debug] {
            let directory = TestDirectory::new("targets");
            emit_with_local_subscriber(directory.path(), level, || {
                tracing::info!(
                    target: "msinsight_platform",
                    "exact_target_marker"
                );
                tracing::info!(
                    target: "msinsight_platform_extra",
                    "prefix_target_marker"
                );
                tracing::info!(
                    target: "msinsight_platform::child",
                    "child_target_marker"
                );
                tracing::info!(
                    target: "third_party",
                    message = REJECTED_SENTINEL
                );
            });

            let output = directory.log_contents();
            assert!(output.contains("exact_target_marker"));
            for rejected in [
                "prefix_target_marker",
                "child_target_marker",
                REJECTED_SENTINEL,
            ] {
                assert!(!output.contains(rejected), "{level:?}: {rejected}");
            }
        }
    }

    #[test]
    fn file_open_failure_returns_payload_free_category() {
        const PRIVATE_PATH_SENTINEL: &str = "PRIVATE_OPEN_PATH_SENTINEL";
        const LOWER_LEVEL_SENTINEL: &str = "PRIVATE_LOWER_LEVEL_SENTINEL";
        let directory = TestDirectory::new(PRIVATE_PATH_SENTINEL);
        let cache_path = directory.path().join(LOWER_LEVEL_SENTINEL);
        fs::write(&cache_path, b"not a directory")
            .expect("create invalid cache path fixture");

        let error = match build_subscriber(
            &cache_path,
            LogLevelControl::new(RustLogLevel::Info),
        ) {
            Ok(_) => panic!("invalid cache path unexpectedly opened"),
            Err(error) => error,
        };

        assert_eq!(error, LoggingInitError::OpenLogFile);
        let exposed = format!("{error:?}|{error}");
        assert!(!exposed.contains(PRIVATE_PATH_SENTINEL));
        assert!(!exposed.contains(LOWER_LEVEL_SENTINEL));
        assert!(error.source().is_none());
    }

    #[test]
    fn global_install_failure_returns_payload_free_category() {
        const CHILD_ENVIRONMENT: &str = "MSINSIGHT_TEST_DOUBLE_INSTALL";
        const TEST_NAME: &str = concat!(
            "logging::tests::",
            "global_install_failure_returns_payload_free_category"
        );
        const PRIVATE_SENTINEL: &str = "PRIVATE_INSTALL_PATH_SENTINEL";

        if has_exact_test_marker(std::env::var_os(CHILD_ENVIRONMENT).as_deref())
        {
            let directory = TestDirectory::new(PRIVATE_SENTINEL);
            assert!(initialize(directory.path()).is_ok());
            let error = match initialize(directory.path()) {
                Ok(_) => panic!("second subscriber unexpectedly installed"),
                Err(error) => error,
            };
            assert_eq!(error, LoggingInitError::InstallSubscriber);
            let exposed = format!("{error:?}|{error}");
            assert!(!exposed.contains(PRIVATE_SENTINEL));
            assert!(error.source().is_none());
            println!("\n{CHILD_COMPLETED}");
            return;
        }

        let output = run_isolated_test(TEST_NAME, CHILD_ENVIRONMENT, &[]);
        let stderr = String::from_utf8_lossy(&output.stderr);
        assert!(!stderr.contains(PRIVATE_SENTINEL));
    }
}
