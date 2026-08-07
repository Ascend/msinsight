/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/coreutils package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/coreutils
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
export type JSONPrimitive = boolean | number | string | null;
export type JSONValue = JSONPrimitive | JSONObject | JSONArray;
export interface JSONObject {
    [key: string]: JSONValue;
}
export interface JSONArray extends Array<JSONValue> {
}
export interface ReadonlyPartialJSONObject {
    readonly [key: string]: ReadonlyPartialJSONValue | undefined;
}
export type ReadonlyPartialJSONValue = JSONPrimitive | ReadonlyPartialJSONObject | ReadonlyPartialJSONArray;
export interface ReadonlyPartialJSONArray extends ReadonlyArray<ReadonlyPartialJSONValue> {
}
export declare namespace JSONExt {
    function isPrimitive(value: ReadonlyPartialJSONValue): value is JSONPrimitive;
    function isArray(value: ReadonlyPartialJSONValue): value is ReadonlyPartialJSONArray;
    function deepEqual(first: ReadonlyPartialJSONValue, second: ReadonlyPartialJSONValue): boolean;
}
