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

import { buildBlockViewPath, getZoom, searchBlockDataByPoint } from '../tools/dataProcess';
import { debounce } from 'lodash';
import { NativeRenderer } from './nativeCanvas/NativeRenderer';
import { store } from '@/store';
import { Session } from '@/entity/session';
import { runInAction } from 'mobx';
import { getCenteredBlockTransform } from '../tools/blockTransform';

export class MainThreadRender {
    canvas: HTMLCanvasElement = document.createElement('canvas');
    memoryBlockData: RenderData = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, blocks: [], reservedLine: [], reservedSizeMax: 0 };
    transform: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
    viewport: RenderOptions['viewport'] = { width: 0, height: 0 };
    zoom: RenderOptions['zoom'] = { x: 1, y: 1, offset: 0 };
    renderer: NativeRenderer | null = null;
    hoverItem: Block | null = null;
    clickItem: Block | null = null;
    session: Session;

    constructor() {
        const { sessionStore } = store;
        this.session = sessionStore.activeSession as Session;
    }

    async initCanvasHandler(payload: Omit<InitCanvasPayload, 'type'>): Promise<void> {
        this.canvas = payload.canvas as HTMLCanvasElement;
        this.renderer = new NativeRenderer(this.canvas, devicePixelRatio);
        this.viewport = { width: payload.width, height: payload.height };
        await this.renderer.initialize();
    };

    setMemoryBlockDataHandler(payload: Omit<SetMemoryBlocksDataPayload, 'type'>): void {
        this.hoverItem = null;
        this.clickItem = null;
        this.memoryBlockData = buildBlockViewPath(payload.data);
        const { maxTimestamp, minTimestamp, minSize } = this.memoryBlockData;
        const maxSize = Math.max(this.memoryBlockData.maxSize, this.memoryBlockData.reservedSizeMax ?? this.memoryBlockData.maxSize);
        this.zoom = getZoom(this.memoryBlockData, this.canvas);
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = {
                maxTimestamp,
                minTimestamp,
                maxSize,
                minSize,
            };
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
        });
        this.renderer?.setZoom(this.zoom).setData(this.memoryBlockData.blocks, this.memoryBlockData.reservedLine);
        this.renderHighlightData();
        this.renderer?.updateCanvasSize(this.viewport);
        runInAction(() => {
            this.session.loadingBlocks = false;
        });
    };

    resizeCanvasHandler(payload: Omit<ResizeCanvasPayload, 'type'>): void {
        this.viewport = { width: payload.width, height: payload.height };
        this.renderer?.updateCanvasSize(this.viewport);
        if (this.memoryBlockData === undefined) {
            return;
        }
        this.zoom = getZoom(this.memoryBlockData, this.canvas);
        this.renderer?.setZoom(this.zoom);
    };

    transformHandler(payload: Omit<TransformPayload, 'type'>): void {
        this.transform = payload.transform;
        this.renderer?.setTransform(this.transform);
    };

    debouncedSearchBlockData = debounce((payload: Omit<HoverItemPayload, 'type'>): void => {
        if (this.memoryBlockData?.blocks?.length > 0) {
            this.hoverItem = searchBlockDataByPoint(this.memoryBlockData, payload, this.transform, this.zoom);
            this.renderHighlightData();
            runInAction(() => {
                this.session.leaksWorkerInfo.hoverItem = this.hoverItem;
            });
        }
    }, 10);

    clickItemHandler(payload: Omit<HoverItemPayload, 'type'>): void {
        this.clickItem = searchBlockDataByPoint(this.memoryBlockData, payload, this.transform, this.zoom);
        runInAction(() => {
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
        this.renderHighlightData();
    };

    selectItemHandler(payload: Omit<SelectBlockItemPayload, 'type'>): void {
        this.clickItem = payload.item;
        this.renderHighlightData();
        runInAction(() => {
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
    };

    selectBlockByIdHandler(payload: Omit<SelectBlockByIdPayload, 'type'>): void {
        this.clickItem = this.memoryBlockData?.blocks?.find(block => block.id === payload.blockId) ?? null;
        const centeredTransform = this.clickItem === null ? null : getCenteredBlockTransform(this.clickItem, this.viewport, this.zoom);
        if (centeredTransform !== null) {
            this.transform = centeredTransform;
            this.renderer?.setTransform(this.transform);
        }
        this.renderHighlightData();
        runInAction(() => {
            this.session.leaksWorkerInfo.renderOptions.transform = this.transform;
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
    };

    hoverItemHandler(payload: Omit<HoverItemPayload, 'type'>): void {
        this.debouncedSearchBlockData(payload);
    };

    renderHighlightData(): void {
        const result: Block[] = [];
        if (this.clickItem !== null) {
            result.push(this.clickItem);
        }
        if (this.hoverItem !== null && this.hoverItem.id !== this.clickItem?.id) {
            result.push(this.hoverItem);
        }
        this.renderer?.setBaseDimmed(this.clickItem !== null);
        this.renderer?.setHighlightData(result);
    };

    destroyHandler(): void {
        this.memoryBlockData = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, blocks: [], reservedLine: [], reservedSizeMax: 0 };
        this.transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        this.zoom = { x: 1, y: 1, offset: 0 };
        this.hoverItem = null;
        this.clickItem = null;
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = {
                maxTimestamp: 0,
                minTimestamp: 0,
                maxSize: 0,
                minSize: 0,
            };
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
            this.session.leaksWorkerInfo.clickItem = null;
            this.session.leaksWorkerInfo.hoverItem = null;
        });
        this.renderHighlightData();
        this.renderer?.setData([]).setTransform(this.transform).setZoom(this.zoom);
    };
}
