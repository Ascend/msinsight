/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/disposable package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/disposable
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
/**
 * An object which implements the disposable pattern.
 */
export interface IDisposable {
    /**
     * Test whether the object has been disposed.
     *
     * #### Notes
     * This property is always safe to access.
     */
    readonly isDisposed: boolean;
    /**
     * Dispose of the resources held by the object.
     *
     * #### Notes
     * If the object's `dispose` method is called more than once, all
     * calls made after the first will be a no-op.
     *
     * #### Undefined Behavior
     * It is undefined behavior to use any functionality of the object
     * after it has been disposed unless otherwise explicitly noted.
     */
    dispose(): void;
}
