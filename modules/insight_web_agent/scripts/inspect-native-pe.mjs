/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

const MACHINES = { 0x14c: "ia32", 0x8664: "x64", 0xaa64: "arm64", 0xa641: "arm64ec" };

export const inspectNativePe = (bytes) => {
    const fail = (message, code = "native_pe_invalid") => { throw Object.assign(new Error(message), { code }); };
    const bounds = (offset, length) => {
        if (!Number.isSafeInteger(offset) || offset < 0 || offset + length > bytes.length) fail("PE field lies outside the file");
        return offset;
    };
    const u16 = (offset) => bytes.readUInt16LE(bounds(offset, 2));
    const u32 = (offset) => bytes.readUInt32LE(bounds(offset, 4));
    if (u16(0) !== 0x5a4d) fail("Native image has no DOS signature");
    const pe = u32(0x3c);
    if (u32(pe) !== 0x4550) fail("Native image has no PE signature");
    const machineCode = u16(pe + 4);
    const sectionCount = u16(pe + 6);
    if (sectionCount > 96) fail("PE section count exceeds inspection limit", "native_pe_inspection_limit");
    const optionalSize = u16(pe + 20);
    const optional = pe + 24;
    bounds(optional, optionalSize);
    const magic = u16(optional);
    const wide = magic === 0x20b;
    if (!wide && magic !== 0x10b) fail("Unsupported PE optional header");
    const directoryBase = wide ? 112 : 96;
    if (optionalSize < directoryBase) fail("Truncated PE optional header");
    const imageBase = wide ? bytes.readBigUInt64LE(bounds(optional + 24, 8)) : BigInt(u32(optional + 28));
    const directoryCount = u32(optional + directoryBase - 4);
    const headersSize = u32(optional + 60);
    const sections = Array.from({ length: sectionCount }, (_, index) => {
        const offset = optional + optionalSize + index * 40;
        bounds(offset, 40);
        return { rva: u32(offset + 12), rawSize: u32(offset + 16), raw: u32(offset + 20) };
    });
    const fileOffset = (rva, size = 1) => {
        if (!Number.isSafeInteger(rva) || rva < 0) fail("Invalid PE relative address");
        if (rva < headersSize && rva + size <= headersSize) return bounds(rva, size);
        const section = sections.find((section) => rva >= section.rva && rva + size <= section.rva + section.rawSize);
        if (!section) fail("PE relative address has no file-backed section");
        return bounds(section.raw + rva - section.rva, size);
    };
    const stringAt = (rva) => {
        const characters = [];
        for (let index = 0; index < 512; index += 1) {
            const value = bytes[fileOffset(rva + index)];
            if (value === 0) return Buffer.from(characters).toString("ascii");
            if (value < 32 || value > 126) fail("PE import name is not printable ASCII");
            characters.push(value);
        }
        fail("PE import name exceeds inspection limit", "native_pe_inspection_limit");
    };
    const directory = (index) => {
        if (index >= directoryCount) return { rva: 0, size: 0 };
        const offset = directoryBase + index * 8;
        if (offset + 8 > optionalSize) fail("PE data directory exceeds optional header");
        return { rva: u32(optional + offset), size: u32(optional + offset + 4) };
    };
    const imports = [];
    let totalSymbols = 0;
    const addImport = (nameRva, thunkRva, kind) => {
        const dll = stringAt(nameRva);
        if (!/^[a-z0-9_.-]+\.(dll|exe)$/i.test(dll)) fail("Unsafe or invalid PE dependency name");
        const symbols = [];
        if (thunkRva) {
            for (let index = 0; ; index += 1) {
                if (index >= 4096 || totalSymbols >= 16384) fail("PE symbol count exceeds inspection limit", "native_pe_inspection_limit");
                const offset = fileOffset(thunkRva + index * (wide ? 8 : 4), wide ? 8 : 4);
                const value = wide ? bytes.readBigUInt64LE(offset) : BigInt(bytes.readUInt32LE(offset));
                if (value === 0n) break;
                const ordinalMask = wide ? 0x8000000000000000n : 0x80000000n;
                if (value & ordinalMask) symbols.push(Number(value & 0xffffn));
                else {
                    if (value > 0xffffffffn) fail("PE symbol name address exceeds RVA range");
                    symbols.push(stringAt(Number(value) + 2));
                }
                totalSymbols += 1;
            }
        }
        imports.push({ dll, kind, symbols });
    };
    for (const [directoryIndex, kind, descriptorSize] of [[1, "normal", 20], [13, "delay", 32]]) {
        const table = directory(directoryIndex);
        if (!table.rva) continue;
        let terminated = false;
        for (let index = 0; index < 128; index += 1) {
            if ((index + 1) * descriptorSize > table.size) fail("PE import descriptors are not terminated");
            const offset = fileOffset(table.rva + index * descriptorSize, descriptorSize);
            const fields = Array.from({ length: descriptorSize / 4 }, (_, field) => u32(offset + field * 4));
            if (fields.every((value) => value === 0)) { terminated = true; break; }
            if (kind === "normal") addImport(fields[3], fields[0] || fields[4], kind);
            else {
                const toRva = (value) => {
                    const rva = fields[0] & 1 ? BigInt(value) : BigInt(value) - imageBase;
                    if (rva < 0n || rva > 0xffffffffn) fail("Invalid delay-import address");
                    return Number(rva);
                };
                addImport(toRva(fields[1]), toRva(fields[4] || fields[3]), kind);
            }
        }
        if (!terminated) fail("PE import descriptor count exceeds inspection limit", "native_pe_inspection_limit");
    }
    return {
        machine: MACHINES[machineCode] ?? "unknown", machineCode: `0x${machineCode.toString(16)}`,
        format: wide ? "PE32+" : "PE32",
        minimumOsVersion: `${u16(optional + 40)}.${u16(optional + 42)}`,
        imports,
    };
};

export const peLogSummary = (pe) => ({
    ...pe,
    imports: pe.imports.map(({ dll, kind, symbols }) => ({ dll, kind, symbolCount: symbols.length, sampleSymbols: symbols.slice(0, 6) })),
});
