/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import test from "node:test";
import { canonicalJson } from "../../../services/rag/wire/canonicalJson.mjs";
import { parseCanonicalJson, parseCanonicalJsonl, pythonFloatRepr } from "../../../services/rag/wire/strictJsonParser.mjs";

test("parseCanonicalJson accepts Python-canonical UTF-8 bytes", () => {
    assert.deepEqual(parseCanonicalJson(Buffer.from('{"a":0.0,"中文":"值"}\n')), { a: 0, 中文: "值" });
});

test("parseCanonicalJson rejects duplicate keys before constructing an object", () => {
    assert.throws(() => parseCanonicalJson('{"a":1,"a":2}\n'), /duplicate object key/);
});

test("parseCanonicalJson rejects BOM, CRLF, whitespace, and unsorted keys", () => {
    for (const value of [
        Buffer.from("efbbbf7b7d0a", "hex"),
        "{}\r\n",
        '{"a": 1}\n',
        '{"b":1,"a":2}\n',
    ]) assert.throws(() => parseCanonicalJson(value), /BOM|CR|canonically encoded|expected a JSON value/);
});

test("parseCanonicalJson rejects truncated documents, malformed escapes, surrogates, and UTF-8", () => {
    for (const value of [
        "",
        "{}",
        "{}\n\n",
        '"\\x"\n',
        '"\\uZZZZ"\n',
        '"unterminated\n',
        '"\\ud800"\n',
        '"\\udc00"\n',
        Buffer.from([0xc3, 0x28, 0x0a]),
    ]) assert.throws(() => parseCanonicalJson(value));
});

test("parseCanonicalJson exercises every JSON value shape and malformed token boundary", () => {
    assert.deepEqual(parseCanonicalJson('{"array":[true,false,null,"text",-2,1.5],"empty":{}}\n'), {
        array: [true, false, null, "text", -2, 1.5],
        empty: {},
    });
    for (const value of ["tru\n", "01\n", "1x\n", "{1:2}\n", "[1 2]\n"]) {
        assert.throws(() => parseCanonicalJson(value));
    }
});

test("parseCanonicalJson rejects unsafe integers and noncanonical float spellings", () => {
    for (const value of ["9007199254740992\n", "1.00\n", "1E-05\n", "0.00001\n"]) {
        assert.throws(() => parseCanonicalJson(value), /safe exact range|canonically encoded/);
    }
    assert.equal(parseCanonicalJson("1\n"), 1);
    assert.equal(parseCanonicalJson("1.0\n"), 1);
});

test("pythonFloatRepr matches the producer Python numeric golden", () => {
    const values = [0, 1, -0, 1.5, 1e-5, 1e-6, 1e-7, 1e15, 1e16, 1e20, 1.2345678901234567, 0.28768207245178085];
    assert.deepEqual(values.map(pythonFloatRepr), [
        "0.0", "1.0", "-0.0", "1.5", "1e-05", "1e-06", "1e-07",
        "1000000000000000.0", "1e+16", "1e+20", "1.2345678901234567", "0.28768207245178085",
    ]);
});

test("parseCanonicalJsonl requires one non-empty canonical record per LF", () => {
    assert.deepEqual(parseCanonicalJsonl('{"a":1}\n{"b":2}\n'), [{ a: 1 }, { b: 2 }]);
    assert.deepEqual(parseCanonicalJsonl(Buffer.alloc(0)), []);
    for (const value of ['{"a":1}\n\n', '{"a":1}', '{"a":1}\r\n']) {
        assert.throws(() => parseCanonicalJsonl(value), /blank|end with LF|CR/);
    }
});

test("parseCanonicalJsonl enforces the per-line byte limit", () => {
    assert.throws(
        () => parseCanonicalJsonl(`"${"x".repeat(400_001)}"\n`),
        /exceeds 400000 bytes/,
    );
});

test("canonicalJson sorts keys by Unicode code point and terminates with LF", () => {
    assert.equal(canonicalJson({ b: 2, a: 1, nested: { z: true, x: null } }), '{"a":1,"b":2,"nested":{"x":null,"z":true}}\n');
    assert.equal(canonicalJson(["x", false, -0, 1.25]), '["x",false,-0.0,1.25]\n');
    for (const value of [Infinity, 9007199254740992, new Map(), undefined]) {
        assert.throws(() => canonicalJson(value));
    }
});
