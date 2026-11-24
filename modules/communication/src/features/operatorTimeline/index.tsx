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
import React, { useEffect, useId, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import { Button, Dropdown, Tooltip } from '@insight/lib/components';
import { ResetIcon } from '@insight/lib/icon';
import { notification } from 'antd';
import { ChartController } from './ChartController';
import { chartHeight } from './options';
import { getChartNavigation, navigateCommunicationChart, resetCommunicationChartZoom, type ChartPointer } from './navigation';
import type { OperatorHit, TimelineProps } from './types';
import { ChartToolbar, ChartHint, ResetZoomButton } from '../../components/chartToolbar';
import './style.css';

const TimelineToolbar = styled(ChartToolbar)({ flexWrap: 'wrap', paddingBottom: 8 });

export type { Highlight } from './types';

/** Page-independent view. Rendering, indexing and chart interactions live beside this component. */
export function OperatorTimeline(props: TimelineProps): JSX.Element {
    const { data, highlight, canLocate, onLocate, onAlign, aligned } = props;
    const theme = useTheme();
    const { t, i18n } = useTranslation('communication');
    const chartRef = useRef<HTMLDivElement>(null);
    const controller = useRef<ChartController>();
    const pointer = useRef<ChartPointer | null>(null);
    const [menu, setMenu] = useState<OperatorHit>();
    const [active, setActive] = useState(false);
    const [zoomed, setZoomed] = useState(false);
    const id = useId();
    const latest = useRef({ props, theme });
    latest.current = { props, theme };

    useEffect(() => {
        const node = chartRef.current;
        if (!node) { return; }
        const resize = (): void => {
            if (node.clientWidth <= 0 || node.clientHeight <= 0) { return; }
            if (controller.current) { controller.current.resize(); return; }
            const current = latest.current;
            controller.current = new ChartController(node, current.props.data, current.theme, {
                onMenu: setMenu,
                onZoom: (range, isZoomed) => { latest.current.props.onRangeChange?.(range); setZoomed(isZoomed); },
            }, current.props.initialRange);
            if (current.props.highlight) { controller.current.highlight(current.props.highlight); }
        };
        const observer = new ResizeObserver(resize);
        observer.observe(node);
        resize();
        // Let ordinary wheel events scroll the page instead of being swallowed by ECharts.
        const wheel = (event: WheelEvent): void => {
            if (!event.ctrlKey && !event.shiftKey) { event.stopPropagation(); } else { event.preventDefault(); }
        };
        node.addEventListener('wheel', wheel, { capture: true, passive: false });
        return () => {
            observer.disconnect();
            node.removeEventListener('wheel', wheel, true);
            controller.current?.dispose();
            controller.current = undefined;
        };
    }, []);
    useEffect(() => { controller.current?.update(data, theme); }, [data, theme, i18n.language]);
    useEffect(() => { controller.current?.setMenuOpen(Boolean(menu)); }, [menu]);
    useEffect(() => {
        if (highlight) { controller.current?.highlight(highlight); } else { controller.current?.clearHighlight(); }
    }, [highlight, data]);
    useEffect(() => {
        if (!highlight || !['allreduce', 'alltoall', 'allgather'].some(name => highlight.name.toLowerCase().replace(/[^a-z]/g, '').includes(name))) { return; }
        const key = `communication-align-${id}`;
        notification.open({
            key,
            className: 'tail-align-notification',
            message: t('alignment.tipTitle'),
            description: t('alignment.tipDescription'),
            btn: <Button size="small" onClick={() => notification.close(key)}>{t('alignment.dismiss')}</Button>,
            duration: 5,
            placement: 'top',
        });
        return () => notification.close(key);
    }, [highlight, id, t]);
    const updatePointer = (event: React.PointerEvent<HTMLDivElement>): void => {
        const rect = event.currentTarget.getBoundingClientRect();
        pointer.current = { x: event.clientX - rect.left, y: event.clientY - rect.top };
    };
    return <div className="operator-timeline">
        <TimelineToolbar>
            <ChartHint id={`${id}-shortcuts`}>{t(active ? 'thumbnailNavigation.activeHint' : 'thumbnailNavigation.focusHint')}</ChartHint>
            <Tooltip title={t('Reset Zoom')}><ResetZoomButton type="text" aria-label={t('Reset Zoom')} disabled={!zoomed}
                icon={<ResetIcon disabled={!zoomed} />}
                onClick={() => { if (controller.current) { resetCommunicationChartZoom(controller.current.chart); } }} /></Tooltip>
        </TimelineToolbar>
        <Dropdown trigger={['contextMenu']} open={Boolean(menu)} onOpenChange={open => { if (!open) { setMenu(undefined); } }} autoFocus menu={{
            items: [
                { key: 'locate', label: t('Find in Timeline'), disabled: !canLocate, onClick: () => { if (menu) { onLocate(menu.row, menu.op); } setMenu(undefined); } },
                { key: 'align', label: t('Align according to selected operator'), onClick: () => { if (menu) { onAlign(menu.op.operatorName); } setMenu(undefined); } },
                { key: 'restore', label: t('Restore default state'), disabled: !aligned, onClick: () => { onAlign(''); setMenu(undefined); } },
            ],
        }}>
            <div ref={chartRef} className="operator-timeline-chart" style={{ height: chartHeight(data.rows.length) }}
                role="region" aria-label={t('sessionTitle.Communication')} aria-describedby={`${id}-shortcuts`}
                aria-keyshortcuts="W S A D Shift+W Shift+S ArrowUp ArrowDown Escape" tabIndex={0}
                onFocus={event => setActive(event.target === event.currentTarget)} onBlur={() => { setActive(false); pointer.current = null; }}
                onPointerMove={updatePointer} onPointerLeave={() => { pointer.current = null; }}
                onPointerDownCapture={event => { if (event.button === 0) { event.currentTarget.focus({ preventScroll: true }); updatePointer(event); } }}
                onContextMenu={event => event.preventDefault()}
                onKeyDown={event => {
                    if (event.target !== event.currentTarget || document.activeElement !== event.currentTarget || menu !== undefined ||
                        event.nativeEvent.isComposing || event.ctrlKey || event.altKey || event.metaKey) { return; }
                    if (event.key === 'Escape') { event.stopPropagation(); event.currentTarget.blur(); return; }
                    const action = getChartNavigation(event.key, event.shiftKey);
                    if (action && controller.current) {
                        event.preventDefault(); event.stopPropagation(); navigateCommunicationChart(controller.current.chart, action, pointer.current);
                    }
                }} />
        </Dropdown>
    </div>;
}
