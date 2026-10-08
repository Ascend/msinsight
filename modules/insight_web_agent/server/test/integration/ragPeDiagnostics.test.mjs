/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { inspectNativePe, peLogSummary } from "../../../scripts/inspect-native-pe.mjs";
import { nativeDiagnosis } from "../../../scripts/summarize-native-diagnosis.mjs";

for (const wide of [false, true]) {
    test(`PE ${wide ? "64" : "32"} inspection maps nonstandard sections and reads named ordinal and delayed imports`, () => {
        const pe = inspectNativePe(peFixture(wide));
        assert.equal(pe.machine, wide ? "x64" : "ia32");
        assert.equal(pe.minimumOsVersion, "6.0");
        assert.deepEqual(pe.imports, [
            { dll: "kernel32.dll", kind: "normal", symbols: ["GetCurrentProcessId", 123] },
            { dll: "node.exe", kind: "delay", symbols: ["napi_create_object"] },
        ]);
        assert.equal(peLogSummary(pe).imports[0].symbolCount, 2);
    });
}

test("PE inspection rejects malformed signatures addresses unsafe names and unterminated tables", () => {
    for (const mutate of [
        (bytes) => bytes.writeUInt16LE(0, 0),
        (bytes) => bytes.writeUInt32LE(0xffffffff, 0x3c),
        (bytes) => bytes.writeUInt32LE(0x40000000, 0x400 + 12),
        (bytes) => bytes.write("../bad.dll\0", 0x600),
        (bytes) => bytes.writeUInt32LE(20, 0x98 + 112 + 8 + 4),
    ]) {
        const bytes = peFixture(true);
        mutate(bytes);
        assert.throws(() => inspectNativePe(bytes), (error) => error.code === "native_pe_invalid");
    }
    assert.throws(() => inspectNativePe(Buffer.alloc(10)), (error) => error.code === "native_pe_invalid");
});

test("PE inspection handles legacy PE32 delay-import virtual addresses", () => {
    const bytes = peFixture(false);
    bytes.writeUInt32LE(0, 0x480);
    bytes.writeUInt32LE(0x10000000 + 0x2250, 0x484);
    bytes.writeUInt32LE(0x10000000 + 0x2380, 0x490);
    assert.equal(inspectNativePe(bytes).imports[1].dll, "node.exe");
});

test("diagnosis keeps 1114 and unavailable CPU feature reports unresolved", () => {
    const diagnosis = nativeDiagnosis([
        { probe: "windows_dll", records: [
            { event: "rag_native_windows_dll_load", loaded: false, winError: 1114, host: "powershell" },
            { event: "rag_native_cpu_features", features: { avx: { reportedAvailable: false } } },
        ] },
    ], { process: { loadedModules: [] } });
    assert.equal(diagnosis.rootCauseStatus, "unresolved");
    assert.ok(diagnosis.unresolved.some(({ code }) => code.includes("1114")));
    assert.ok(diagnosis.confirmedFacts.some(({ code }) => code === "cpu_feature_api_results"));
    assert.ok(diagnosis.candidates.some((value) => value.includes("requires_validation")));
});

test("diagnosis detects digest and original-host differences without calling them proven root causes", () => {
    const diagnosis = nativeDiagnosis([
        { probe: "node_source", records: [{ event: "rag_native_files", files: [{ path: "binding.node", sha256: "a" }] }, { event: "rag_native_import", outcome: "passed" }] },
        { probe: "node_bundle", records: [{ event: "rag_native_files", files: [{ path: "binding.node", sha256: "b" }] }, { event: "rag_native_import", outcome: "passed", loadedModules: [{ name: "MSVCP140.dll", path: "C:\\Windows\\System32\\MSVCP140.dll" }] }] },
    ], { process: { loadedModules: [{ name: "MSVCP140.dll", path: "C:\\other\\MSVCP140.dll" }] } });
    assert.equal(diagnosis.rootCauseStatus, "unresolved");
    assert.ok(diagnosis.confirmedFacts.some(({ code }) => code === "native_bytes_differ"));
    assert.ok(diagnosis.confirmedFacts.some(({ code }) => code === "runtime_module_paths_differ"));
    assert.ok(diagnosis.candidates.includes("original_process_context_or_transient_failure"));
});

test("diagnosis identifies a failed import with a mismatched native image architecture", () => {
    const diagnosis = nativeDiagnosis([{ probe: "node_bundle", records: [
        { event: "rag_native_files", arch: "x64", files: [{ path: "binding.node", pe: { machine: "ia32" } }] },
        { event: "rag_native_import", outcome: "failed", error: { code: "ERR_DLOPEN_FAILED" } },
    ] }]);
    assert.equal(diagnosis.rootCauseStatus, "identified_architecture_mismatch");
});

test("diagnosis excludes optional delayed dependencies from mandatory import failures", () => {
    const diagnosis = nativeDiagnosis([{ probe: "windows_dll", records: [
        { event: "rag_native_dependency", dll: "optional.dll", loaded: false, requiredForImport: false },
        { event: "rag_native_dependency", dll: "required.dll", loaded: true, missingSymbolCount: 1, missingSymbols: ["Missing"], requiredForImport: true },
    ] }]);
    const failures = diagnosis.confirmedFacts.filter(({ code }) => code === "dependency_failure_in_controlled_probe");
    assert.equal(failures.length, 1);
    assert.equal(failures[0].dll, "required.dll");
    assert.equal(diagnosis.rootCauseStatus, "unresolved");
});

function peFixture(wide) {
    const bytes = Buffer.alloc(0x2000);
    const optional = 0x98;
    const optionalSize = wide ? 240 : 224;
    const directory = optional + (wide ? 112 : 96);
    const rva = (offset) => 0x2000 + offset - 0x400;
    bytes.writeUInt16LE(0x5a4d, 0);
    bytes.writeUInt32LE(0x80, 0x3c);
    bytes.writeUInt32LE(0x4550, 0x80);
    bytes.writeUInt16LE(wide ? 0x8664 : 0x14c, 0x84);
    bytes.writeUInt16LE(1, 0x86);
    bytes.writeUInt16LE(optionalSize, 0x94);
    bytes.writeUInt16LE(wide ? 0x20b : 0x10b, optional);
    if (wide) bytes.writeBigUInt64LE(0x180000000n, optional + 24);
    else bytes.writeUInt32LE(0x10000000, optional + 28);
    bytes.writeUInt16LE(6, optional + 40);
    bytes.writeUInt32LE(0x400, optional + 60);
    bytes.writeUInt32LE(16, directory - 4);
    const section = optional + optionalSize;
    bytes.write("oddname", section);
    bytes.writeUInt32LE(0x2000, section + 12);
    bytes.writeUInt32LE(0x1800, section + 16);
    bytes.writeUInt32LE(0x400, section + 20);
    bytes.writeUInt32LE(rva(0x400), directory + 8);
    bytes.writeUInt32LE(40, directory + 12);
    bytes.writeUInt32LE(rva(0x480), directory + 13 * 8);
    bytes.writeUInt32LE(64, directory + 13 * 8 + 4);
    bytes.writeUInt32LE(rva(0x700), 0x400);
    bytes.writeUInt32LE(rva(0x600), 0x40c);
    bytes.write("kernel32.dll\0", 0x600);
    bytes.writeUInt32LE(1, 0x480);
    bytes.writeUInt32LE(rva(0x650), 0x484);
    bytes.writeUInt32LE(rva(0x780), 0x490);
    bytes.write("node.exe\0", 0x650);
    bytes.write("GetCurrentProcessId\0", 0x902);
    bytes.write("napi_create_object\0", 0x982);
    if (wide) {
        bytes.writeBigUInt64LE(BigInt(rva(0x900)), 0x700);
        bytes.writeBigUInt64LE(0x800000000000007bn, 0x708);
        bytes.writeBigUInt64LE(BigInt(rva(0x980)), 0x780);
    } else {
        bytes.writeUInt32LE(rva(0x900), 0x700);
        bytes.writeUInt32LE(0x8000007b, 0x704);
        bytes.writeUInt32LE(rva(0x980), 0x780);
    }
    return bytes;
}
