/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/algorithm package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/algorithm
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
export declare function map<T, U>(object: Iterable<T>, fn: (value: T, index: number) => U): IterableIterator<U>;
export declare function find<T>(object: Iterable<T>, fn: (value: T, index: number) => boolean): T | undefined;
/**
 * The namespace for array-specific algorithms.
 */
export declare namespace ArrayExt {
    function findFirstIndex<T>(list: ArrayLike<T>, predicate: (item: T, index: number) => boolean, begin?: number, end?: number): number;
    function removeAllWhere<T>(items: T[], predicate: (item: T, index: number) => boolean, from?: number, to?: number): number;
}
