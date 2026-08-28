[CmdletBinding()]
param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Read-RegistryState {
    param([Microsoft.Win32.RegistryView]$View, [string]$SubKey)

    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $View
    )
    try {
        $key = $base.OpenSubKey($SubKey, $false)
        if ($null -eq $key) {
            return [ordered]@{ view = $View.ToString(); key = $SubKey; exists = $false; values = @{} }
        }
        try {
            $values = [ordered]@{}
            foreach ($name in @("InstallPath", "Version", "DisplayVersion", "UninstallString")) {
                $value = $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                if ($null -ne $value) { $values[$name] = [string]$value }
            }
            return [ordered]@{ view = $View.ToString(); key = $SubKey; exists = $true; values = $values }
        } finally {
            $key.Dispose()
        }
    } finally {
        $base.Dispose()
    }
}

function Test-Elevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not $IsWindows -and $env:OS -ne "Windows_NT") {
    $result = [ordered]@{ schemaVersion = "1.0"; status = "blocked"; elevated = $false; blockers = @("windows_required"); registry = @(); paths = @(); processes = @() }
    $result | ConvertTo-Json -Depth 8 -Compress:$Json
    exit 2
}

$registryKeys = @(
    "Software\huawei\MindStudio Insight",
    "Software\Microsoft\Windows\CurrentVersion\Install\MindStudio Insight",
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight"
)
$registry = foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64)) {
    foreach ($key in $registryKeys) { Read-RegistryState -View $view -SubKey $key }
}

$candidatePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $registry) {
    if ($entry.values.InstallPath) { [void]$candidatePaths.Add([IO.Path]::GetFullPath($entry.values.InstallPath)) }
}
foreach ($programFiles in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
    if ($programFiles) { [void]$candidatePaths.Add([IO.Path]::Combine($programFiles, "MindStudio Insight")) }
}

$paths = foreach ($path in ($candidatePaths | Sort-Object)) {
    $exists = Test-Path -LiteralPath $path -PathType Container
    $desktop = Test-Path -LiteralPath (Join-Path $path "MindStudio-Insight.exe") -PathType Leaf
    $active = Test-Path -LiteralPath (Join-Path $path "resources\profiler\server\insight_web_agent\rag-data\active.json") -PathType Leaf
    $classification = if (-not $exists) { "absent" } elseif ($desktop -and $active) { "valid-install" } else { "incomplete-directory" }
    [ordered]@{ path = $path; exists = $exists; classification = $classification }
}

$processes = @()
try {
    foreach ($process in Get-CimInstance Win32_Process) {
        $name = [string]$process.Name
        $commandLine = [string]$process.CommandLine
        $executable = [string]$process.ExecutablePath
        $isProduct = $name -in @("MindStudio-Insight.exe", "ascend_insight.exe", "profiler_server.exe")
        $isRelatedNode = $name -eq "node.exe" -and ($commandLine -match "(?i)MindStudio Insight|insight_web_agent")
        if ($isProduct -or $isRelatedNode) {
            $classification = if ($isProduct -or $executable -or $commandLine) { "related" } else { "unclassified" }
            $processes += [ordered]@{ processId = [int]$process.ProcessId; name = $name; classification = $classification }
        }
    }
} catch {
    $processes += [ordered]@{ processId = 0; name = "process-enumeration"; classification = "unclassified" }
}

$blockers = [Collections.Generic.List[string]]::new()
$elevated = Test-Elevated
if (-not $elevated) { $blockers.Add("elevation_required") }
if ($paths.classification -contains "valid-install") { $blockers.Add("valid_install_present") }
if ($paths.classification -contains "incomplete-directory") { $blockers.Add("incomplete_product_directory") }
if (($registry.exists -contains $true) -and -not ($paths.exists -contains $true)) { $blockers.Add("stale_registry") }
if ($processes.Count -gt 0) { $blockers.Add("related_or_unclassified_process") }

$result = [ordered]@{
    schemaVersion = "1.0"
    status = if ($blockers.Count -eq 0) { "clean" } else { "blocked" }
    elevated = $elevated
    blockers = @($blockers | Sort-Object -Unique)
    registry = @($registry)
    paths = @($paths)
    processes = @($processes)
}
$result | ConvertTo-Json -Depth 8 -Compress:$Json
if ($result.status -ne "clean") { exit 2 }
