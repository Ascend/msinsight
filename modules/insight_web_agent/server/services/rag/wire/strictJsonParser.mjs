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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

const UTF8 = new TextDecoder("utf-8", { fatal: true, ignoreBOM: false });
const NUMBER_PREFIX = /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/;
const MAX_JSONL_LINE_BYTES = 400_000;
const OBJECT_KEY_ORDER = new WeakMap();

export class StrictJsonError extends Error {
    constructor(code, message, { label = "JSON", offset } = {}) {
        super(`${label}: ${message}${offset === undefined ? "" : ` at offset ${offset}`}`);
        this.name = "StrictJsonError";
        this.code = code;
        this.label = label;
        this.offset = offset;
    }
}

/** Parse one Python-canonical JSON document terminated by exactly one LF. */
export const parseCanonicalJson = (input, label = "JSON") => {
    const bytes = toBytes(input);
    if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
        fail("strict_json_invalid", "UTF-8 BOM is not allowed", { label, offset: 0 });
    }
    if (!bytes.length || bytes.at(-1) !== 0x0a) {
        fail("noncanonical_json", "document must end with exactly one LF", { label });
    }
    if (bytes.length > 1 && bytes.at(-2) === 0x0a) {
        fail("noncanonical_json", "document must not contain a trailing blank line", { label });
    }
    const payload = decodeUtf8(bytes.subarray(0, -1), label);
    if (payload.includes("\r")) fail("noncanonical_json", "CR is not allowed", { label });
    const node = new Parser(payload, label).parse();
    if (node.canonical !== payload) {
        fail("noncanonical_json", "document is not canonically encoded", { label });
    }
    return node.value;
};

/** Parse canonical JSONL. Empty bytes are the sole zero-record encoding. */
export const parseCanonicalJsonl = (input, label = "JSONL") => {
    const bytes = toBytes(input);
    if (!bytes.length) return [];
    if (bytes.includes(0x0d)) fail("noncanonical_json", "CR is not allowed", { label });
    if (bytes.at(-1) !== 0x0a) {
        fail("noncanonical_json", "JSONL must end with LF", { label });
    }
    const records = [];
    let start = 0;
    let line = 1;
    for (let index = 0; index < bytes.length; index += 1) {
        if (bytes[index] !== 0x0a) continue;
        const row = bytes.subarray(start, index);
        if (!row.length) fail("noncanonical_json", `blank JSONL line ${line} is not allowed`, { label });
        if (row.length > MAX_JSONL_LINE_BYTES) {
            fail("strict_json_invalid", `JSONL line ${line} exceeds ${MAX_JSONL_LINE_BYTES} bytes`, { label });
        }
        records.push(parseCanonicalJson(Buffer.concat([row, Buffer.from("\n")]), `${label} line ${line}`));
        start = index + 1;
        line += 1;
    }
    return records;
};

/** Preserve producer key order even for JavaScript integer-like property names. */
export const strictObjectKeys = (value) => OBJECT_KEY_ORDER.get(value) ?? Object.keys(value);

class Parser {
    constructor(text, label) {
        this.text = text;
        this.label = label;
        this.offset = 0;
    }

    parse() {
        const node = this.value();
        if (this.offset !== this.text.length) this.error("unexpected trailing data");
        return node;
    }

    value() {
        const current = this.text[this.offset];
        if (current === "{") return this.object();
        if (current === "[") return this.array();
        if (current === "\"") return this.string();
        if (current === "t") return this.literal("true", true);
        if (current === "f") return this.literal("false", false);
        if (current === "n") return this.literal("null", null);
        if (current === "-" || isDigit(current)) return this.number();
        this.error("expected a JSON value");
    }

    object() {
        this.offset += 1;
        const entries = [];
        const keys = new Set();
        if (this.consume("}")) return { value: {}, canonical: "{}" };
        while (true) {
            if (this.text[this.offset] !== "\"") this.error("object key must be a string");
            const key = this.string();
            if (keys.has(key.value)) this.error(`duplicate object key ${JSON.stringify(key.value)}`);
            keys.add(key.value);
            this.expect(":");
            const value = this.value();
            entries.push([key, value]);
            if (this.consume("}")) break;
            this.expect(",");
        }
        const object = Object.fromEntries(entries.map(([key, value]) => [key.value, value.value]));
        OBJECT_KEY_ORDER.set(object, entries.map(([key]) => key.value));
        const sortedEntries = [...entries].sort(([left], [right]) => compareCodePoints(left.value, right.value));
        return {
            value: object,
            canonical: `{${sortedEntries.map(([key, value]) => `${key.canonical}:${value.canonical}`).join(",")}}`,
        };
    }

    array() {
        this.offset += 1;
        const values = [];
        if (this.consume("]")) return { value: [], canonical: "[]" };
        while (true) {
            values.push(this.value());
            if (this.consume("]")) break;
            this.expect(",");
        }
        return {
            value: values.map(({ value }) => value),
            canonical: `[${values.map(({ canonical }) => canonical).join(",")}]`,
        };
    }

    string() {
        const start = this.offset;
        this.offset += 1;
        while (this.offset < this.text.length) {
            const code = this.text.charCodeAt(this.offset);
            if (code === 0x22) {
                this.offset += 1;
                const token = this.text.slice(start, this.offset);
                let value;
                try {
                    value = JSON.parse(token);
                } catch {
                    this.error("invalid JSON string escape", start);
                }
                rejectUnpairedSurrogates(value, this.label, start);
                return { value, canonical: JSON.stringify(value) };
            }
            if (code < 0x20) this.error("unescaped control character in string");
            if (code !== 0x5c) {
                this.offset += 1;
                continue;
            }
            this.offset += 1;
            const escaped = this.text[this.offset];
            if ("\"\\/bfnrt".includes(escaped)) {
                this.offset += 1;
                continue;
            }
            if (escaped !== "u" || !/^[0-9a-fA-F]{4}$/.test(this.text.slice(this.offset + 1, this.offset + 5))) {
                this.error("invalid JSON string escape");
            }
            this.offset += 5;
        }
        this.error("unterminated JSON string", start);
    }

    number() {
        const start = this.offset;
        const token = NUMBER_PREFIX.exec(this.text.slice(start))?.[0];
        if (!token) this.error("invalid JSON number");
        this.offset += token.length;
        const next = this.text[this.offset];
        if (next !== undefined && !",]}".includes(next)) this.error("invalid JSON number");
        const value = Number(token);
        if (!Number.isFinite(value)) this.error("JSON number is not finite", start);
        const isFloat = token.includes(".") || /e/i.test(token);
        if (!isFloat && !Number.isSafeInteger(value)) {
            this.error("JSON integer is outside the safe exact range", start);
        }
        const canonical = isFloat ? pythonFloatRepr(value) : String(value);
        return { value, canonical };
    }

    literal(token, value) {
        if (!this.text.startsWith(token, this.offset)) this.error(`invalid literal ${token}`);
        this.offset += token.length;
        return { value, canonical: token };
    }

    expect(character) {
        if (!this.consume(character)) this.error(`expected ${JSON.stringify(character)}`);
    }

    consume(character) {
        if (this.text[this.offset] !== character) return false;
        this.offset += 1;
        return true;
    }

    error(message, offset = this.offset) {
        fail("strict_json_invalid", message, { label: this.label, offset });
    }
}

/**
 * Python and ECMAScript use the same shortest-roundtrip digits but switch to
 * scientific notation at different decimal exponents. Package v4 is authored
 * by Python, so normalize the ECMAScript digits to Python's observable spelling.
 */
export const pythonFloatRepr = (value) => {
    if (!Number.isFinite(value)) throw new TypeError("canonical JSON does not support non-finite numbers");
    if (Object.is(value, -0)) return "-0.0";
    const sign = value < 0 ? "-" : "";
    const { digits, exponent } = decimalDigits(Math.abs(value));
    if (exponent < -4 || exponent >= 16) {
        const fraction = digits.length > 1 ? `.${digits.slice(1)}` : "";
        const exponentSign = exponent >= 0 ? "+" : "-";
        return `${sign}${digits[0]}${fraction}e${exponentSign}${String(Math.abs(exponent)).padStart(2, "0")}`;
    }
    const point = exponent + 1;
    let fixed;
    if (point <= 0) fixed = `0.${"0".repeat(-point)}${digits}`;
    else if (point >= digits.length) fixed = `${digits}${"0".repeat(point - digits.length)}`;
    else fixed = `${digits.slice(0, point)}.${digits.slice(point)}`;
    return `${sign}${fixed.includes(".") ? fixed : `${fixed}.0`}`;
};

const decimalDigits = (value) => {
    if (value === 0) return { digits: "0", exponent: 0 };
    const text = value.toString().toLowerCase();
    if (text.includes("e")) {
        const [mantissa, exponentText] = text.split("e");
        return {
            digits: trimSignificantZeros(mantissa.replace(".", "")),
            exponent: Number(exponentText),
        };
    }
    if (text.startsWith("0.")) {
        const fraction = text.slice(2);
        const first = fraction.search(/[1-9]/);
        return {
            digits: trimSignificantZeros(fraction.slice(first)),
            exponent: -(first + 1),
        };
    }
    const point = text.indexOf(".");
    const integerLength = point < 0 ? text.length : point;
    return {
        digits: trimSignificantZeros(text.replace(".", "")),
        exponent: integerLength - 1,
    };
};

const trimSignificantZeros = (digits) => digits.replace(/^0+/, "").replace(/0+$/, "") || "0";

const compareCodePoints = (left, right) => {
    const leftPoints = Array.from(left, (value) => value.codePointAt(0));
    const rightPoints = Array.from(right, (value) => value.codePointAt(0));
    const count = Math.min(leftPoints.length, rightPoints.length);
    for (let index = 0; index < count; index += 1) {
        if (leftPoints[index] !== rightPoints[index]) return leftPoints[index] - rightPoints[index];
    }
    return leftPoints.length - rightPoints.length;
};

const rejectUnpairedSurrogates = (value, label, offset) => {
    for (let index = 0; index < value.length; index += 1) {
        const code = value.charCodeAt(index);
        if (code >= 0xd800 && code <= 0xdbff) {
            const low = value.charCodeAt(index + 1);
            if (!(low >= 0xdc00 && low <= 0xdfff)) {
                fail("strict_json_invalid", "string contains an unpaired surrogate", { label, offset });
            }
            index += 1;
        } else if (code >= 0xdc00 && code <= 0xdfff) {
            fail("strict_json_invalid", "string contains an unpaired surrogate", { label, offset });
        }
    }
};

const decodeUtf8 = (bytes, label) => {
    try {
        return UTF8.decode(bytes);
    } catch (error) {
        throw new StrictJsonError("strict_json_invalid", "content is not valid UTF-8", { label, offset: 0, cause: error });
    }
};

const toBytes = (input) => Buffer.isBuffer(input)
    ? input
    : input instanceof Uint8Array
        ? Buffer.from(input.buffer, input.byteOffset, input.byteLength)
        : Buffer.from(String(input ?? ""), "utf8");

const isDigit = (value) => value !== undefined && value >= "0" && value <= "9";

const fail = (code, message, details) => {
    throw new StrictJsonError(code, message, details);
};
