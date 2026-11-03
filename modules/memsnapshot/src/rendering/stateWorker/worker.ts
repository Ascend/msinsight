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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { isWebGL2Supported } from '../../common/leaksWorker/tools/detection';
import { MainThreadRender } from './mainThreadindex';
import type { MemSnapshotContext } from '../../state/MemSnapshotContext';
import { createRendererRegistry } from '../../common/shared/workers/RendererRegistry';
import { createNativeWorker } from './createWorker';

declare const module: { hot?: { dispose: (callback: () => void) => void } };

export interface StateRenderer {
    dispose: () => void;
    subscribe: (listener: (event: MessageEvent<any>) => void) => () => void;
    isCurrentGeneration: (generation: number | undefined) => boolean;
    initCanvas: (payload: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>) => void;
    setMemoryStateData: (payload: Omit<SetMemoryStateDataPayload, 'type'>) => void;
    resizeCanvas: (payload: Omit<ResizeCanvasPayload, 'type'>) => void;
    transform: (payload: Omit<TransformPayload, 'type'>) => void;
    hoverItem: (payload: Omit<HoverItemPayload, 'type'>) => void;
    clickItem: (payload: Omit<ClickItemPayload, 'type'>) => void;
    selectItem: (payload: Omit<SelectStateItemPayload, 'type'>) => void;
    destroy: () => void;
}

export const createStateRenderer = (session: MemSnapshotContext, useWorker: boolean = isWebGL2Supported()): StateRenderer => {
    const ownerModule = session.module;
    let disposed = false;
    const isOwnerCurrent = (): boolean => !disposed && session.module === ownerModule;
    let nativeWorker: Worker | undefined;
    let mainThreadRender: MainThreadRender | undefined;
    const listeners = new Set<(event: MessageEvent<any>) => void>();
    const transferredCanvases = new WeakSet<HTMLCanvasElement>();
    const getWorker = (): Worker => {
        if (!nativeWorker) {
            nativeWorker = createNativeWorker();
        }
        return nativeWorker;
    };
    const getMainThreadRender = (): MainThreadRender => {
        if (!mainThreadRender) { mainThreadRender = new MainThreadRender(); mainThreadRender.session = session; mainThreadRender.isSessionCurrent = isOwnerCurrent; }
        return mainThreadRender;
    };

    let stateGeneration = 0;
    const postStateMessage = (payload: object, transfer: Transferable[] = []): void => {
        getWorker().postMessage({ ...payload, generation: stateGeneration }, transfer);
    };

    // Worker 实现
    const WorkerBackend = {
        initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
            const devicePixelRatio = window.devicePixelRatio || 1;
            if (transferredCanvases.has(canvas as HTMLCanvasElement)) return;
            const offscreenCanvas = (canvas as HTMLCanvasElement).transferControlToOffscreen();
            transferredCanvases.add(canvas as HTMLCanvasElement);
            postStateMessage(
                { type: 'initCanvas', canvas: offscreenCanvas, devicePixelRatio, width, height },
                [offscreenCanvas],
            );
        },
        setMemoryStateData({ data }: Omit<SetMemoryStateDataPayload, 'type'>): void {
            stateGeneration++;
            postStateMessage({ type: 'setMemoryStateData', data });
        },
        resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
            postStateMessage({ type: 'resizeCanvas', width, height });
        },
        transform({ transform }: Omit<TransformPayload, 'type'>): void {
            postStateMessage({ type: 'transform', transform });
        },
        hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
            postStateMessage({ type: 'hoverItem', clientX, clientY });
        },
        clickItem({ clientX, clientY, selectionVersion }: Omit<ClickItemPayload, 'type'>): void {
            postStateMessage({ type: 'clickItem', clientX, clientY, selectionVersion });
        },
        selectItem({ item, selectionVersion }: Omit<SelectStateItemPayload, 'type'>): void {
            postStateMessage({ type: 'selectStateItem', item, selectionVersion });
        },
        destroy(): void {
            stateGeneration++;
            postStateMessage({ type: 'destroy' });
        },
    };

    // 主线程实现（fallback）
    const MainThreadBackend = {
        initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
            const devicePixelRatio = window.devicePixelRatio || 1;
            getMainThreadRender().initCanvasHandler({ canvas, devicePixelRatio, width, height });
        },
        setMemoryStateData({ data }: Omit<SetMemoryStateDataPayload, 'type'>): void {
            getMainThreadRender().setMemoryStateDataHandler({ data });
        },
        resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
            getMainThreadRender().resizeCanvasHandler({ width, height });
        },
        transform({ transform }: Omit<TransformPayload, 'type'>): void {
            getMainThreadRender().transformHandler({ transform });
        },
        hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
            getMainThreadRender().hoverItemHandler({ clientX, clientY });
        },
        clickItem({ clientX, clientY }: Omit<ClickItemPayload, 'type'>): void {
            getMainThreadRender().clickItemHandler({ clientX, clientY });
        },
        selectItem({ item }: Omit<SelectStateItemPayload, 'type'>): void {
            getMainThreadRender().selectItemHandler({ item });
        },
        destroy(): void {
            getMainThreadRender().destroyHandler();
        },
    };

    // 自动选择后端
    const backend = useWorker ? WorkerBackend : MainThreadBackend;

    const dispose = (): void => {
        if (disposed) return;
        disposed = true;
        stateGeneration++;

        for (const listener of listeners) nativeWorker?.removeEventListener('message', listener);
        listeners.clear();
        if (nativeWorker) {
            nativeWorker.terminate();

            nativeWorker = undefined;
        }
        const retiringMainThread = mainThreadRender;
        if (retiringMainThread) {
            retiringMainThread.session = undefined;
            retiringMainThread.destroyHandler(); mainThreadRender = undefined;
        }
    };

    return {
        dispose,
        isCurrentGeneration: (generation: number | undefined): boolean => isOwnerCurrent() && (generation === stateGeneration),
        subscribe: (listener: (event: MessageEvent<any>) => void): (() => void) => {
            if (!isOwnerCurrent() || !useWorker) return () => undefined;
            const currentWorker = getWorker();
            const ownedListener = (event: MessageEvent<any>): void => {
                if (isOwnerCurrent()) listener(event);
            };
            listeners.add(ownedListener);
            currentWorker.addEventListener('message', ownedListener);
            return () => { listeners.delete(ownedListener); currentWorker.removeEventListener('message', ownedListener); };
        },
        initCanvas: (payload: Parameters<typeof backend.initCanvas>[0]) => !isOwnerCurrent() ? undefined : backend.initCanvas(payload),
        setMemoryStateData: (payload: Parameters<typeof backend.setMemoryStateData>[0]) => !isOwnerCurrent() ? undefined : backend.setMemoryStateData(payload),
        resizeCanvas: (payload: Parameters<typeof backend.resizeCanvas>[0]) => !isOwnerCurrent() ? undefined : backend.resizeCanvas(payload),
        transform: (payload: Parameters<typeof backend.transform>[0]) => !isOwnerCurrent() ? undefined : backend.transform(payload),
        hoverItem: (payload: Parameters<typeof backend.hoverItem>[0]) => !isOwnerCurrent() ? undefined : backend.hoverItem(payload),
        clickItem: (payload: Parameters<typeof backend.clickItem>[0]) => !isOwnerCurrent() ? undefined : backend.clickItem(payload),
        selectItem: (payload: Parameters<typeof backend.selectItem>[0]) => !isOwnerCurrent() ? undefined : backend.selectItem(payload),
        destroy: () => !isOwnerCurrent() || (!nativeWorker && !mainThreadRender) ? undefined : backend.destroy(),
    };
};

const renderers = createRendererRegistry(createStateRenderer);
export const getStateRenderer = renderers.get;
module.hot?.dispose(renderers.disposeAll);
export const bindStateWorkerSession = (session: MemSnapshotContext | undefined): (() => void) => session ? renderers.retain(session) : () => undefined;
export const subscribeStateWorker = (session: MemSnapshotContext, listener: (event: MessageEvent<any>) => void): (() => void) => renderers.get(session).subscribe(listener);
export const isCurrentStateWorkerGeneration = (generation: number | undefined, session: MemSnapshotContext): boolean => renderers.peek(session)?.isCurrentGeneration(generation) ?? false;
export const workerInitCanvas = (payload: Parameters<StateRenderer['initCanvas']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['initCanvas']> => renderers.get(session).initCanvas(payload);
export const workerSetMemoryStateData = (payload: Parameters<StateRenderer['setMemoryStateData']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['setMemoryStateData']> => renderers.get(session).setMemoryStateData(payload);
export const workerResizeCanvas = (payload: Parameters<StateRenderer['resizeCanvas']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['resizeCanvas']> => renderers.get(session).resizeCanvas(payload);
export const workerTransform = (payload: Parameters<StateRenderer['transform']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['transform']> => renderers.get(session).transform(payload);
export const workerHoverItem = (payload: Parameters<StateRenderer['hoverItem']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['hoverItem']> => renderers.get(session).hoverItem(payload);
export const workerClickItem = (payload: Parameters<StateRenderer['clickItem']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['clickItem']> => renderers.get(session).clickItem(payload);
export const workerSelectItem = (payload: Parameters<StateRenderer['selectItem']>[0], session: MemSnapshotContext): ReturnType<StateRenderer['selectItem']> => renderers.get(session).selectItem(payload);
export const workerDestroy = (session: MemSnapshotContext | undefined): ReturnType<StateRenderer['destroy']> => session ? renderers.peek(session)?.destroy() : undefined;
