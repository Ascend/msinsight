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
import type { ECharts } from 'echarts';
import type { Theme } from '@emotion/react';
import { getAdaptiveEchart, disposeAdaptiveEchart } from '@insight/lib/utils';
import type { Timeline } from '../../domain/types';
import {
    buildCommunicationWebGLIndex, selectVisibleCommunicationOperators, findCommunicationOperator,
    type CommunicationWebGLIndex, type CommunicationWebGLOperator,
} from './dataIndex';
import { createCommunicationTimeWebGLRenderer, type CommunicationTimeWebGLRenderer, type CommunicationWebGLLayout } from './WebGLRenderer';
import { getWebGLLayout, getWebGLViewport, hitTest } from './geometry';
import { chartOptions } from './options';
import { palette } from './colors';
import { OperatorTooltip } from './tooltip';
import type { Highlight, OperatorHit } from './types';
import type { Range } from './navigation';

interface Callbacks {
    onMenu: (hit: OperatorHit) => void;
    onZoom: (range: Range, zoomed: boolean) => void;
}
type Zoom = Array<{ start?: number; end?: number }>;

/** Owns chart resources and event handlers, with no dependency on page state or backend APIs. */
export class ChartController {
    readonly chart: ECharts;
    private renderer: CommunicationTimeWebGLRenderer | null;
    private readonly tooltip: OperatorTooltip;
    private index: CommunicationWebGLIndex;
    private layout: CommunicationWebGLLayout | null = null;
    private frame: number | undefined;
    private highlightTimer: number | undefined;
    private selectedId: number | null = null;
    private hoveredId: number | null = null;
    private disposed = false;
    private menuOpen = false;

    constructor(private readonly container: HTMLElement, private data: Timeline, private theme: Theme,
        private readonly callbacks: Callbacks, initialRange?: Range) {
        this.index = buildCommunicationWebGLIndex(data);
        this.chart = getAdaptiveEchart(container);
        this.renderer = createCommunicationTimeWebGLRenderer(container, () => this.fallback());
        this.tooltip = new OperatorTooltip(container);
        container.dataset.renderer = this.renderer ? 'webgl' : 'echarts';
        this.chart.on('rendered', this.scheduleRender);
        this.chart.on('datazoom', this.handleZoom);
        this.chart.getZr().on('mousemove', this.handleMove);
        this.chart.getZr().on('globalout', this.hideTooltip);
        this.chart.getZr().on('contextmenu', this.handleContextMenu);
        this.chart.setOption(chartOptions(this.index, theme, Boolean(this.renderer), initialRange), true);
        this.handleZoom();
    }

    update(data: Timeline, theme: Theme): void {
        if (data === this.data && theme === this.theme) { return; }
        const zoom = this.zoom();
        if (data !== this.data) {
            this.clearHighlight();
            this.index = buildCommunicationWebGLIndex(data);
            this.data = data;
        }
        this.theme = theme;
        this.hideTooltip();
        this.chart.setOption(chartOptions(this.index, theme, Boolean(this.renderer)), true);
        this.restoreZoom(zoom);
        if (!this.renderer && this.selectedId !== null) {
            this.chart.dispatchAction({ type: 'highlight', seriesIndex: 0, name: String(this.selectedId) });
        }
    }

    resize(): void { this.chart.resize(); this.hideTooltip(); }
    setMenuOpen(open: boolean): void { this.menuOpen = open; if (open) { this.hideTooltip(); } }

    private zoom(): Zoom { return (this.chart.getOption().dataZoom as Zoom).slice(0, 2); }
    private restoreZoom(zoom: Zoom): void {
        this.chart.dispatchAction({ type: 'dataZoom', batch: zoom.map(({ start, end }, dataZoomIndex) => ({ dataZoomIndex, start, end })) });
    }

    private readonly handleZoom = (): void => {
        const zoom = this.zoom();
        this.callbacks.onZoom([zoom[0]?.start ?? 0, zoom[0]?.end ?? 100],
            zoom.some(({ start = 0, end = 100 }) => start > 0 || end < 100));
        this.hideTooltip();
        this.scheduleRender();
    };

    private readonly scheduleRender = (): void => {
        if (this.disposed || this.frame !== undefined) { return; }
        this.frame = window.requestAnimationFrame(() => {
            this.frame = undefined;
            this.layout = getWebGLLayout(this.container, this.chart, this.index);
            if (!this.renderer) { return; }
            if (!this.layout) { this.renderer.clear(); return; }
            this.renderer.setColors(palette.map(color => this.theme.colorPalette[color]));
            this.renderer.setData(selectVisibleCommunicationOperators(this.index, getWebGLViewport(this.layout, this.index)), this.layout);
        });
    };

    private hit(x: number, y: number): CommunicationWebGLOperator | null {
        // Read the latest axes even if a pointer event arrives before the scheduled WebGL frame.
        this.layout = getWebGLLayout(this.container, this.chart, this.index);
        return hitTest(this.index, this.layout, x, y);
    }

    private readonly handleMove = (event: { offsetX: number; offsetY: number }): void => {
        if (this.menuOpen) { return; }
        const operator = this.hit(event.offsetX, event.offsetY);
        if (!operator) { this.hideTooltip(); return; }
        if (this.hoveredId !== operator.id) {
            this.hideTooltip();
            this.hoveredId = operator.id;
            if (this.renderer) { this.renderer.setHoveredOperator(operator.id); } else {
                this.chart.dispatchAction({ type: 'highlight', seriesIndex: 0, name: String(operator.id) });
            }
        }
        this.tooltip.show(operator, this.index.ranks[operator.rankIndex].rankId,
            this.theme.colorPalette[palette[operator.colorIndex]], event.offsetX, event.offsetY);
    };

    private readonly hideTooltip = (): void => {
        this.tooltip.hide();
        this.renderer?.setHoveredOperator(null);
        if (!this.renderer && this.hoveredId !== null && this.hoveredId !== this.selectedId) {
            this.chart.dispatchAction({ type: 'downplay', seriesIndex: 0, name: String(this.hoveredId) });
        }
        this.hoveredId = null;
    };

    private readonly handleContextMenu = (event: { offsetX: number; offsetY: number; event?: Event }): void => {
        event.event?.preventDefault();
        const operator = this.hit(event.offsetX, event.offsetY);
        this.hideTooltip();
        if (operator) { this.callbacks.onMenu({ row: this.index.ranks[operator.rankIndex].row, op: operator.original }); }
    };

    highlight(value: Highlight): void {
        this.clearHighlight();
        const rankIndex = this.index.rankIndexById.get(value.rankId);
        if (rankIndex === undefined) { return; }
        const count = this.index.ranks.length;
        this.chart.dispatchAction({
            type: 'dataZoom',
            batch: [
                { dataZoomIndex: 0, startValue: value.startValue, endValue: value.endValue },
                {
                    dataZoomIndex: 1,
                    ...(count < 20
                        ? { start: 0, end: 100 }
                        : {
                            startValue: this.index.ranks[Math.max(0, rankIndex - 10)].rankId,
                            endValue: this.index.ranks[Math.min(count - 1, rankIndex + 10)].rankId,
                        }),
                },
            ],
        });
        this.selectedId = findCommunicationOperator(this.index, value.rankId, value.name, value.startValue)?.id ?? null;
        if (this.renderer) { this.renderer.setSelectedOperator(this.selectedId); } else if (this.selectedId !== null) {
            this.chart.dispatchAction({ type: 'highlight', seriesIndex: 0, name: String(this.selectedId) });
        }
        this.scheduleRender();
        this.highlightTimer = window.setTimeout(() => this.clearHighlight(), 5000);
    }

    clearHighlight(): void {
        window.clearTimeout(this.highlightTimer);
        this.highlightTimer = undefined;
        this.renderer?.setSelectedOperator(null);
        if (!this.renderer && this.selectedId !== null) {
            this.chart.dispatchAction({ type: 'downplay', seriesIndex: 0, name: String(this.selectedId) });
        }
        this.selectedId = null;
    }

    private fallback(): void {
        if (this.disposed || !this.renderer) { return; }
        const zoom = this.zoom();
        this.renderer.dispose();
        this.renderer = null;
        this.container.dataset.renderer = 'echarts';
        this.chart.setOption(chartOptions(this.index, this.theme, false), true);
        this.restoreZoom(zoom);
        if (this.selectedId !== null) { this.chart.dispatchAction({ type: 'highlight', seriesIndex: 0, name: String(this.selectedId) }); }
    }

    dispose(): void {
        this.disposed = true;
        window.cancelAnimationFrame(this.frame ?? 0);
        window.clearTimeout(this.highlightTimer);
        this.chart.off('rendered', this.scheduleRender);
        this.chart.off('datazoom', this.handleZoom);
        this.chart.getZr().off('mousemove', this.handleMove);
        this.chart.getZr().off('globalout', this.hideTooltip);
        this.chart.getZr().off('contextmenu', this.handleContextMenu);
        this.renderer?.dispose();
        this.tooltip.dispose();
        disposeAdaptiveEchart(this.container);
        delete this.container.dataset.renderer;
    }
}
