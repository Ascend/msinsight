# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
# MindStudio is licensed under Mulan PSL v2.

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$nativeDll = $env:MSINSIGHT_RAG_PROBE_DLL
$nodeDirectory = $env:MSINSIGHT_RAG_PROBE_NODE_DIR

function Write-ProbeRecord($record) {
    [Console]::WriteLine(($record | ConvertTo-Json -Compress -Depth 6))
}

$nativeProbeError = $false
# Emit loader evidence before potentially slow CIM or PATH queries.
try {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class RagNativeLoadResult {
    public bool Loaded;
    public int WinError;
    public string Message;
    public string[] LoadedModules;
}

public class RagDependencyResult {
    public bool Loaded;
    public int WinError;
    public string Path;
    public string[] MissingSymbols;
}

public static class RagNativeLoader {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandleW(string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern uint GetModuleFileNameW(IntPtr handle, StringBuilder path, int size);
    [DllImport("kernel32.dll")]
    private static extern bool FreeLibrary(IntPtr handle);
    [DllImport("kernel32.dll")]
    private static extern uint SetErrorMode(uint mode);
    [DllImport("kernel32.dll")]
    public static extern bool IsProcessorFeaturePresent(uint feature);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32.dll", EntryPoint = "GetProcAddress", ExactSpelling = true)]
    private static extern IntPtr GetProcAddressOrdinal(IntPtr module, IntPtr ordinal);

    public static RagDependencyResult CheckDependency(string path, string[] names, int[] ordinals) {
        IntPtr handle = LoadLibraryExW(path, IntPtr.Zero, System.IO.Path.IsPathRooted(path) ? 0x900u : 0x800u);
        int error = handle == IntPtr.Zero ? Marshal.GetLastWin32Error() : 0;
        var missing = new List<string>();
        string loadedPath = null;
        if (handle != IntPtr.Zero) {
            var filename = new StringBuilder(4096);
            GetModuleFileNameW(handle, filename, filename.Capacity);
            loadedPath = filename.ToString();
            foreach (string name in names) if (GetProcAddress(handle, name) == IntPtr.Zero) missing.Add(name);
            foreach (int ordinal in ordinals) if (GetProcAddressOrdinal(handle, new IntPtr(ordinal)) == IntPtr.Zero) missing.Add("#" + ordinal);
            FreeLibrary(handle);
        }
        return new RagDependencyResult {Loaded = handle != IntPtr.Zero, WinError = error, Path = loadedPath, MissingSymbols = missing.ToArray()};
    }

    public static RagNativeLoadResult Probe(string path) {
        SetErrorMode(0x8003);
        // Initialize the DLL using its own directory and System32 only.
        IntPtr handle = LoadLibraryExW(path, IntPtr.Zero, 0x00000900);
        int error = handle == IntPtr.Zero ? Marshal.GetLastWin32Error() : 0;
        var modules = new List<string>();
        foreach (string name in new[] {"onnxruntime.dll", "msvcp140.dll", "msvcp140_1.dll", "vcruntime140.dll", "vcruntime140_1.dll", "ucrtbase.dll"}) {
            IntPtr module = GetModuleHandleW(name);
            if (module == IntPtr.Zero) continue;
            var filename = new StringBuilder(4096);
            if (GetModuleFileNameW(module, filename, filename.Capacity) != 0) modules.Add(filename.ToString());
        }
        if (handle != IntPtr.Zero) FreeLibrary(handle);
        return new RagNativeLoadResult {Loaded = handle != IntPtr.Zero, WinError = error,
            Message = error == 0 ? null : new Win32Exception(error).Message, LoadedModules = modules.ToArray()};
    }
}
'@
    $result = [RagNativeLoader]::Probe($nativeDll)
    Write-ProbeRecord @{
        event = 'rag_native_windows_dll_load'
        host = 'powershell'
        searchPolicy = 'dll-directory+System32'
        library = [IO.Path]::GetFileName($nativeDll)
        loaded = $result.Loaded
        winError = $result.WinError
        message = $result.Message
        loadedModules = $result.LoadedModules
    }
    $features = @{}
    foreach ($feature in @(
        @{ name = 'sse2'; id = 10 }, @{ name = 'sse3'; id = 13 }, @{ name = 'xsave'; id = 17 },
        @{ name = 'ssse3'; id = 36 }, @{ name = 'sse41'; id = 37 }, @{ name = 'sse42'; id = 38 },
        @{ name = 'avx'; id = 39 }, @{ name = 'avx2'; id = 40 }, @{ name = 'avx512f'; id = 41 }
    )) {
        $features[$feature.name] = @{ featureId = $feature.id; reportedAvailable = [RagNativeLoader]::IsProcessorFeaturePresent($feature.id) }
    }
    Write-ProbeRecord @{
        event = 'rag_native_cpu_features'; method = 'IsProcessorFeaturePresent'; features = $features
        interpretation = 'False can mean unavailable or undetectable. Feature IDs 36-41 require Windows 10 build 19041 or later. These results alone do not prove an ONNX CPU requirement.'
    }
} catch {
    Write-ProbeRecord @{ event = 'rag_native_windows_dll_load'; outcome = 'probe_error'; message = $_.Exception.Message }
    $nativeProbeError = $true
}

# Inspect the actual imported functions, including API-set names resolved by Windows.
# Each dependency is checked independently and does not substitute for Node loading.
try {
    $requestText = [Console]::In.ReadToEnd()
    if (-not [string]::IsNullOrWhiteSpace($requestText)) {
        $request = $requestText | ConvertFrom-Json
        foreach ($file in $request.files) {
            if ($file.error) {
                Write-ProbeRecord @{ event = 'rag_native_pe_check_error'; file = $file.path; error = $file.error }
                continue
            }
            foreach ($dependency in $file.imports) {
                if ($dependency.dll -notmatch '^[a-zA-Z0-9_.-]+\.(dll|exe)$') {
                    Write-ProbeRecord @{ event = 'rag_native_dependency'; file = $file.path; dll = $dependency.dll; outcome = 'invalid_dependency_name' }
                    continue
                }
                if ($dependency.dll -match '\.exe$') {
                    Write-ProbeRecord @{ event = 'rag_native_dependency'; file = $file.path; dll = $dependency.dll; kind = $dependency.kind; outcome = 'host_bound'; reason = 'Executable imports must be evaluated in their owning host process' }
                    continue
                }
                $candidate = Join-Path ([IO.Path]::GetDirectoryName($nativeDll)) $dependency.dll
                if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { $candidate = $dependency.dll }
                $names = @($dependency.symbols | Where-Object { $_ -is [string] })
                $ordinals = @($dependency.symbols | Where-Object { $_ -isnot [string] } | ForEach-Object { [int]$_ })
                $check = [RagNativeLoader]::CheckDependency($candidate, [string[]]$names, [int[]]$ordinals)
                Write-ProbeRecord @{
                    event = 'rag_native_dependency'; file = $file.path; dll = $dependency.dll; kind = $dependency.kind
                    host = 'powershell'; searchPolicy = 'native-candidate-or-System32'
                    loaded = $check.Loaded; winError = $check.WinError; resolvedPath = $check.Path
                    importedSymbolCount = @($dependency.symbols).Count
                    missingSymbolCount = @($check.MissingSymbols).Count
                    missingSymbols = @($check.MissingSymbols | Select-Object -First 32)
                    requiredForImport = $dependency.kind -eq 'normal'
                }
            }
        }
    }
} catch {
    Write-ProbeRecord @{ event = 'rag_native_dependencies_error'; message = $_.Exception.Message }
}

try {
    $os = Get-CimInstance Win32_OperatingSystem -OperationTimeoutSec 5
    $cpu = @(Get-CimInstance Win32_Processor -OperationTimeoutSec 5 | ForEach-Object {
        @{ name = $_.Name; architecture = $_.Architecture; addressWidth = $_.AddressWidth }
    })
    Write-ProbeRecord @{
        event = 'rag_native_windows_environment'
        windows = @{ caption = $os.Caption; version = $os.Version; build = $os.BuildNumber }
        cpu = $cpu
        processBitness = [IntPtr]::Size * 8
        powershell = $PSVersionTable.PSVersion.ToString()
    }
} catch {
    Write-ProbeRecord @{ event = 'rag_native_windows_environment'; outcome = 'probe_error'; message = $_.Exception.Message }
}

try {
    $runtimeNames = @('msvcp140.dll', 'msvcp140_1.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'ucrtbase.dll')
    $directories = @(
        @{ role = 'system'; path = [Environment]::SystemDirectory },
        @{ role = 'node'; path = $nodeDirectory },
        @{ role = 'native'; path = [IO.Path]::GetDirectoryName($nativeDll) }
    )
    $inventory = @()
    foreach ($directory in $directories) {
        foreach ($name in $runtimeNames) {
            $path = Join-Path $directory.path $name
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                $file = Get-Item -LiteralPath $path
                $inventory += @{ role = $directory.role; name = $name; path = $path; exists = $true; version = $file.VersionInfo.FileVersion; sizeBytes = $file.Length }
            } else {
                $inventory += @{ role = $directory.role; name = $name; exists = $false }
            }
        }
    }
    # Report only matching runtime DLLs, never the complete PATH or environment.
    $pathMatches = @()
    foreach ($directory in @($env:PATH -split ';' | Select-Object -Unique | Select-Object -First 32)) {
        if ([string]::IsNullOrWhiteSpace($directory)) { continue }
        foreach ($name in $runtimeNames) {
            try {
                $path = Join-Path $directory.Trim('"') $name
                if (Test-Path -LiteralPath $path -PathType Leaf) {
                    $file = Get-Item -LiteralPath $path
                    $pathMatches += @{ name = $name; path = $path; version = $file.VersionInfo.FileVersion }
                }
            } catch { }
        }
    }
    Write-ProbeRecord @{ event = 'rag_native_windows_runtimes'; files = $inventory; pathMatches = @($pathMatches | Select-Object -First 32) }
} catch {
    Write-ProbeRecord @{ event = 'rag_native_windows_runtimes'; outcome = 'probe_error'; message = $_.Exception.Message }
}

if ($nativeProbeError) { exit 1 }
