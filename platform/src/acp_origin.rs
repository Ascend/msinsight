/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

pub(crate) fn acp_allowed_origins_for(os: &str) -> &'static [&'static str] {
    match os {
        "macos" => &["wry://localhost"],
        "windows" => &["http://wry.localhost"],
        // Legacy Wry omits Origin on Linux but still requires a CORS response.
        "linux" => &["wry://localhost", "*"],
        _ => &[],
    }
}

pub(crate) fn acp_allowed_origins() -> &'static [&'static str] {
    acp_allowed_origins_for(std::env::consts::OS)
}

#[cfg(test)]
mod tests {
    #[test]
    fn desktop_acp_origins_stay_exact_and_do_not_widen_windows() {
        assert_eq!(super::acp_allowed_origins_for("macos"), &["wry://localhost"]);
        assert_eq!(
            super::acp_allowed_origins_for("windows"),
            &["http://wry.localhost"]
        );
        assert_eq!(
            super::acp_allowed_origins_for("linux"),
            &["wry://localhost", "*"]
        );
        assert!(!super::acp_allowed_origins_for("windows").contains(&"*"));
        assert!(!super::acp_allowed_origins_for("macos").contains(&"*"));
        assert_eq!(
            super::acp_allowed_origins(),
            super::acp_allowed_origins_for(std::env::consts::OS)
        );
    }
}
