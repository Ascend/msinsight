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

import { getMemoryStateRenderData, getMemoryStateZoom, searchStateDataByPoint } from '../tools/dataProcess';
import { debounce } from 'lodash';
import { NativeRenderer } from './nativeCanvas/NativeRenderer';
import { store } from '@/store';
import { Session } from '@/entity/session';
import { runInAction } from 'mobx';
export class MainThreadRender {
    canvas: HTMLCanvasElement = document.createElement('canvas');
    memoryStateData: Segment[] = [];
    transform: RenderOptions['transform'] = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
    viewport: RenderOptions['viewport'] = { width: 0, height: 0 };
    zoom: RenderOptions['zoom'] = { x: 1, y: 1, offset: 0 };
    renderer: NativeRenderer | null = null;

    hoverItem: StateDataHoverResult | null = null;
    clickItem: StateDataHoverResult | null = null;
    session: Session;

    constructor() {
        const { sessionStore } = store;
        this.session = sessionStore.activeSession as Session;
    }

    async initCanvasHandler(payload: Omit<InitCanvasPayload, 'type'>): Promise<void> {
        this.canvas = payload.canvas as HTMLCanvasElement;
        this.renderer = new NativeRenderer(this.canvas, devicePixelRatio);
        this.canvas.width = payload.width;
        this.canvas.height = payload.height;
        this.viewport = { width: payload.width, height: payload.height };
        await this.renderer.initialize();
    };

    setMemoryStateDataHandler(payload: Omit<SetMemoryStateDataPayload, 'type'>): void {
        this.clickItem = null;
        this.hoverItem = null;
        this.renderHighlightData();
        this.memoryStateData = getMemoryStateRenderData(payload.data);
        this.zoom = getMemoryStateZoom(this.memoryStateData, this.canvas);
        this.renderer?.setZoom(this.zoom).setData(this.memoryStateData);
        this.renderer?.updateCanvasSize(this.viewport);
    };

    resizeCanvasHandler(payload: Omit<ResizeCanvasPayload, 'type'>): void {
        this.viewport = { width: payload.width, height: payload.height };
        this.renderer?.updateCanvasSize(this.viewport);
        if (this.memoryStateData === undefined) {
            return;
        }
        this.zoom = getMemoryStateZoom(this.memoryStateData, this.canvas);
        this.renderer?.setZoom(this.zoom);
    };

    transformHandler(payload: Omit<TransformPayload, 'type'>): void {
        this.transform = payload.transform;
        this.renderer?.setTransform(this.transform);
    };

    debouncedSearchBlockData = debounce((payload: Omit<HoverItemPayload, 'type'>): void => {
        if (this.memoryStateData?.length > 0) {
            this.hoverItem = searchStateDataByPoint(this.memoryStateData, payload, this.transform, this.zoom);
            this.renderHighlightData();
            runInAction(() => {
                this.session.stateWorkerInfo.hoverItem = this.hoverItem;
            });
        }
    }, 10);

    clickItemHandler(payload: Omit<HoverItemPayload, 'type'>): void {
        this.clickItem = searchStateDataByPoint(this.memoryStateData, payload, this.transform, this.zoom);
        runInAction(() => {
            this.session.stateWorkerInfo.clickItem = this.clickItem;
        });
        this.renderHighlightData();
    };

    selectItemHandler(payload: Omit<SelectStateItemPayload, 'type'>): void {
        this.clickItem = payload.item;
        this.renderHighlightData();
        runInAction(() => {
            this.session.stateWorkerInfo.clickItem = this.clickItem;
        });
    };

    hoverItemHandler(payload: Omit<HoverItemPayload, 'type'>): void {
        this.debouncedSearchBlockData(payload);
    };

    // 通过这个方法，优先高亮hover数据
    renderHighlightData(): void {
        const result = this.getHighlightData();
        this.renderer?.setBaseDimmed(this.clickItem !== null);
        this.renderer?.setHighlightData(result);
    };

    isSameHighlightItem(a: StateDataHoverResult | null, b: StateDataHoverResult | null): boolean {
        if (a === null || b === null || a.type !== b.type) {
            return false;
        }
        if (a.type === 'segment') {
            return a.data.allocOrMapEventId === b.data.allocOrMapEventId;
        }
        return a.data.blocks[0]?.id === b.data.blocks[0]?.id;
    };

    getHighlightData(): StateDataHoverResult[] {
        const result: StateDataHoverResult[] = [];
        if (this.clickItem !== null) {
            result.push(this.clickItem);
        }
        if (this.hoverItem !== null && !this.isSameHighlightItem(this.hoverItem, this.clickItem)) {
            result.push(this.hoverItem);
        }
        return result;
    };

    destroyHandler(): void {
        this.memoryStateData = [];
        this.transform = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
        this.zoom = { x: 1, y: 1, offset: 0 };
        this.clickItem = null;
        this.hoverItem = null;
        this.renderHighlightData();
        runInAction(() => {
            this.session.stateWorkerInfo.clickItem = null;
            this.session.stateWorkerInfo.hoverItem = null;
        });
        this.renderer?.setData([]).setTransform(this.transform).setZoom(this.zoom);
    };
}
