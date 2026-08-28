[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][ValidatePattern("^[0-9a-f]{64}$")][string]$ExpectedInstallerSha256,
    [Parameter(Mandatory = $true)][switch]$ApproveElevatedInstall
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$preflightScript = Join-Path $scriptRoot "fresh-install-preflight.ps1"

if (-not $ApproveElevatedInstall) { throw "Explicit elevated-install approval is required" }
if (-not (Test-Path -LiteralPath $Installer -PathType Leaf)) { throw "Installer is missing" }
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$actualSha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $ExpectedInstallerSha256) { throw "Installer SHA-256 does not match approval input" }

$preflightText = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $preflightScript -Json 2>$null
$preflightExit = $LASTEXITCODE
$preflight = $preflightText | ConvertFrom-Json
if ($preflightExit -ne 0 -or $preflight.status -ne "clean" -or -not $preflight.elevated) {
    throw "Fresh-install preflight is not clean and elevated"
}

$process = Start-Process -FilePath $installerPath -ArgumentList "/S" -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Installer failed" }

$installPaths = foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64)) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
    try {
        $key = $base.OpenSubKey("Software\Microsoft\Windows\CurrentVersion\Install\MindStudio Insight", $false)
        if ($null -ne $key) {
            try { [string]$key.GetValue("InstallPath") } finally { $key.Dispose() }
        }
    } finally {
        $base.Dispose()
    }
}
$installPath = @($installPaths | Where-Object { $_ } | Select-Object -Unique)
if ($installPath.Count -ne 1) { throw "Installed path identity is missing or ambiguous" }

$bundleRoot = Join-Path $installPath[0] "resources\profiler\server\insight_web_agent"
$node = (Get-Command node.exe -CommandType Application -ErrorAction Stop).Source
& $node (Join-Path $bundleRoot "rag-cli.mjs") verify
if ($LASTEXITCODE -ne 0) { throw "Installed RAG verify failed" }
& $node (Join-Path $bundleRoot "rag-required-smoke.mjs")
if ($LASTEXITCODE -ne 0) { throw "Installed required RAG smoke failed" }

[ordered]@{
    schemaVersion = "1.0"
    status = "installed-verification-passed"
    installerSha256 = $actualSha256
    installPath = $installPath[0]
} | ConvertTo-Json -Compress
