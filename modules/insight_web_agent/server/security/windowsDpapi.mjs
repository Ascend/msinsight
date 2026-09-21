/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { execFileSync } from "node:child_process";

const POWERSHELL = process.env.SystemRoot
    ? `${process.env.SystemRoot}\\System32\\WindowsPowerShell\\v1.0\\powershell.exe`
    : "powershell.exe";

export const protectCurrentUser = (payload) => runDpapi("Protect", payload);

export const unprotectCurrentUser = (payload) => runDpapi("Unprotect", payload);

const runDpapi = (operation, payload) => {
    const encoded = Buffer.from(payload).toString("base64");
    const stdout = execFileSync(POWERSHELL, [
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        `Add-Type -AssemblyName System.Security; `
        + `$bytes = [Convert]::FromBase64String('${encoded}'); `
        + `$scope = [System.Security.Cryptography.DataProtectionScope]::CurrentUser; `
        + `$result = [System.Security.Cryptography.ProtectedData]::${operation}($bytes, $null, $scope); `
        + `[Convert]::ToBase64String($result)`,
    ], {
        encoding: "utf8",
        timeout: 10000,
        windowsHide: true,
    });
    return Buffer.from(String(stdout).trim(), "base64");
};
