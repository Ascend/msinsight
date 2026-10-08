/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const nativeDiagnosis = (probes, primaryFailure) => {
    const confirmedFacts = [];
    const candidates = [];
    const unresolved = [];
    const probe = (name) => probes.find((value) => value.probe === name);
    const record = (name, event) => probe(name)?.records?.find((value) => value?.event === event);
    const sourceFiles = record("node_source", "rag_native_files")?.files;
    const bundleInventory = record("node_bundle", "rag_native_files");
    const bundleFiles = bundleInventory?.files;
    let rootCauseStatus = "unresolved";

    for (const name of ["node_source", "node_bundle", "windows_dll"]) {
        const value = probe(name);
        if (value && ["probe_error", "not_run"].includes(value.outcome)) unresolved.push({ code: "probe_incomplete", probe: name, reason: value.error?.code ?? value.reason });
    }
    if (sourceFiles?.length && bundleFiles?.length && sourceFiles.length === bundleFiles.length
        && bundleFiles.every((file) => file.sha256 && sourceFiles.some((source) => source.path === file.path && source.sha256))) {
        const changed = bundleFiles.filter((file) => sourceFiles.find((source) => source.path === file.path).sha256 !== file.sha256).map(({ path }) => path);
        confirmedFacts.push({ code: changed.length ? "native_bytes_differ" : "native_bytes_identical", ...(changed.length ? { files: changed } : {}) });
        if (changed.length) candidates.push("artifact_difference_requires_investigation");
    } else unresolved.push({ code: "native_digest_comparison_incomplete" });

    const sourceImport = record("node_source", "rag_native_import");
    const bundleImport = record("node_bundle", "rag_native_import");
    for (const [scope, result] of [["source", sourceImport], ["bundle", bundleImport]]) {
        if (result) confirmedFacts.push({ code: "isolated_node_import", scope, outcome: result.outcome, phase: result.phase, errorCode: result.error?.code });
    }
    if (primaryFailure && sourceImport?.outcome === "passed" && bundleImport?.outcome === "passed") {
        candidates.push("original_process_context_or_transient_failure");
        unresolved.push({ code: "isolated_success_does_not_explain_original_failure" });
    }
    const normalizePath = (path) => String(path).replace(/^\\\\\?\\/, "").replaceAll("\\", "/").toLowerCase();
    const originalModules = primaryFailure?.process?.loadedModules ?? [];
    const isolatedModules = bundleImport?.loadedModules;
    if (Array.isArray(isolatedModules)) {
        for (const original of originalModules.filter(({ name }) => /^(?:msvcp140|vcruntime140|ucrtbase)/i.test(name))) {
            const isolated = isolatedModules.find(({ name }) => name.toLowerCase() === original.name.toLowerCase());
            if (isolated && normalizePath(isolated.path) !== normalizePath(original.path)) {
                confirmedFacts.push({ code: "runtime_module_paths_differ", module: original.name, originalPath: original.path, isolatedPath: isolated.path });
                candidates.push("runtime_search_path_or_preload_difference");
            }
        }
    }
    for (const file of bundleFiles ?? []) {
        if (file.pe?.error) unresolved.push({ code: "pe_inspection_incomplete", file: file.path, errorCode: file.pe.error.code });
        const machine = file.pe?.machine;
        if (["ia32", "x64", "arm64"].includes(machine) && bundleInventory.arch && machine !== bundleInventory.arch) {
            confirmedFacts.push({ code: "native_architecture_mismatch", file: file.path, machine, nodeArch: bundleInventory.arch });
            if (bundleImport?.outcome === "failed" && bundleImport.error?.code === "ERR_DLOPEN_FAILED") rootCauseStatus = "identified_architecture_mismatch";
        }
    }

    const core = record("windows_dll", "rag_native_windows_dll_load");
    if (probe("windows_dll") && typeof core?.loaded !== "boolean") unresolved.push({ code: "standalone_core_dll_result_missing" });
    for (const value of probe("windows_dll")?.records ?? []) {
        if (value.outcome === "probe_error" || /_error$/.test(value.event ?? "")) {
            unresolved.push({ code: "windows_probe_step_incomplete", event: value.event, errorCode: value.error?.code });
        }
    }
    if (typeof core?.loaded === "boolean") {
        confirmedFacts.push({ code: "standalone_core_dll_load", loaded: core.loaded, winError: core.winError, host: core.host });
        if (core.winError === 1114) {
            candidates.push("native_initialization_exception_or_dependency_initialization");
            unresolved.push({ code: "winerror_1114_does_not_identify_the_failing_initializer", nextEvidence: "Windows loader trace or native exception stack" });
        }
    }
    for (const dependency of probe("windows_dll")?.records ?? []) {
        if (dependency.event !== "rag_native_dependency" || !dependency.requiredForImport) continue;
        if (dependency.loaded === false || dependency.missingSymbolCount > 0) {
            confirmedFacts.push({ code: "dependency_failure_in_controlled_probe", dll: dependency.dll, file: dependency.file,
                winError: dependency.winError, missingSymbols: dependency.missingSymbols, resolvedPath: dependency.resolvedPath });
            candidates.push("dependency_or_export_compatibility_requires_original_host_validation");
        }
    }
    const features = record("windows_dll", "rag_native_cpu_features")?.features;
    if (features) confirmedFacts.push({ code: "cpu_feature_api_results", features });
    if (features?.avx?.reportedAvailable === false || features?.avx2?.reportedAvailable === false) {
        candidates.push("cpu_or_os_feature_reporting_requires_validation_against_binary_requirements");
    }
    if (primaryFailure && !primaryFailure.process) unresolved.push({ code: "original_process_snapshot_missing" });
    return {
        event: "rag_native_diagnosis", rootCauseStatus, confirmedFacts,
        candidates: [...new Set(candidates)], unresolved,
        interpretation: "Facts describe observed processes and search policies. Candidates are not confirmed root causes; CPU flags alone do not establish ONNX requirements.",
    };
};
