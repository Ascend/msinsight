/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/signaling package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/signaling
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
export interface ISignal<T, U> {
    connect(slot: Slot<T, U>, thisArg?: any): boolean;
    disconnect(slot: Slot<T, U>, thisArg?: any): boolean;
}
export type Slot<T, U> = (sender: T, args: U) => void;
export declare class Signal<T, U> implements ISignal<T, U> {
    constructor(sender: T);
    readonly sender: T;
    connect(slot: Slot<T, U>, thisArg?: unknown): boolean;
    disconnect(slot: Slot<T, U>, thisArg?: unknown): boolean;
    emit(args: U): void;
}
/**
 * The namespace for the `Signal` class statics.
 */
export declare namespace Signal {
    function disconnectBetween(sender: unknown, receiver: unknown): void;
    function disconnectSender(sender: unknown): void;
    function disconnectReceiver(receiver: unknown): void;
    function disconnectAll(object: unknown): void;
    function clearData(object: unknown): void;
    type ExceptionHandler = (err: Error) => void;
}
