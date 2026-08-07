/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/coreutils package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/coreutils
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
export var JSONExt;
(function (JSONExt) {
    function isPrimitive(value) {
        return (value === null ||
            typeof value === 'number' ||
            typeof value === 'string' ||
            typeof value === 'boolean');
    }
    JSONExt.isPrimitive = isPrimitive;
    function isArray(value) {
        return Array.isArray(value);
    }
    JSONExt.isArray = isArray;
    function deepEqual(first, second) {
        if (first === second) {
            return true;
        }
        if (isPrimitive(first) || isPrimitive(second)) {
            return false;
        }
        let a1 = isArray(first);
        let a2 = isArray(second);
        if (a1 !== a2) {
            return false;
        }
        if (a1 && a2) {
            return deepArrayEqual(first, second);
        }
        return deepObjectEqual(first, second);
    }
    JSONExt.deepEqual = deepEqual;
    function deepArrayEqual(first, second) {
        if (first === second) {
            return true;
        }
        if (first.length !== second.length) {
            return false;
        }
        for (let i = 0, n = first.length; i < n; ++i) {
            if (!deepEqual(first[i], second[i])) {
                return false;
            }
        }
        return true;
    }
    function deepObjectEqual(first, second) {
        if (first === second) {
            return true;
        }
        for (let key in first) {
            if (first[key] !== undefined && !(key in second)) {
                return false;
            }
        }
        for (let key in second) {
            if (second[key] !== undefined && !(key in first)) {
                return false;
            }
        }
        for (let key in first) {
            if (Object.prototype.hasOwnProperty.call(first, key)) {
                let firstValue = first[key];
                let secondValue = second[key];
                if (firstValue === undefined && secondValue === undefined) {
                    continue;
                }
                if (firstValue === undefined || secondValue === undefined) {
                    return false;
                }
                if (!deepEqual(firstValue, secondValue)) {
                    return false;
                }
            }
        }
        return true;
    }
})(JSONExt || (JSONExt = {}));
//# sourceMappingURL=coreutils.js.map