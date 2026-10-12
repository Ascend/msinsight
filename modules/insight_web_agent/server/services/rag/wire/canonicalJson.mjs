/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { pythonFloatRepr } from "./strictJsonParser.mjs";

export const canonicalJson = (value) => `${serialize(value)}\n`;
export const canonicalJsonBytes = (value) => Buffer.from(canonicalJson(value), "utf8");

const serialize = (value) => {
    if (value === null) return "null";
    if (typeof value === "boolean") return value ? "true" : "false";
    if (typeof value === "string") return JSON.stringify(value);
    if (typeof value === "number") {
        if (!Number.isFinite(value)) throw new TypeError("canonical JSON does not support non-finite numbers");
        if (Number.isInteger(value) && !Object.is(value, -0)) {
            if (!Number.isSafeInteger(value)) throw new TypeError("canonical JSON integer is outside the safe exact range");
            return String(value);
        }
        return pythonFloatRepr(value);
    }
    if (Array.isArray(value)) return `[${value.map(serialize).join(",")}]`;
    if (!value || typeof value !== "object" || Object.getPrototypeOf(value) !== Object.prototype) {
        throw new TypeError("canonical JSON supports only plain objects and arrays");
    }
    const entries = Object.keys(value).sort(compareCodePoints).map((key) => `${JSON.stringify(key)}:${serialize(value[key])}`);
    return `{${entries.join(",")}}`;
};

const compareCodePoints = (left, right) => {
    const leftPoints = Array.from(left, (value) => value.codePointAt(0));
    const rightPoints = Array.from(right, (value) => value.codePointAt(0));
    const count = Math.min(leftPoints.length, rightPoints.length);
    for (let index = 0; index < count; index += 1) {
        if (leftPoints[index] !== rightPoints[index]) return leftPoints[index] - rightPoints[index];
    }
    return leftPoints.length - rightPoints.length;
};
