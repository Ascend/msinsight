/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';

const Pane = styled.div`
    position: relative;
    display: flex;
    flex: 1;
    min-width: 0;
    min-height: 0;
    > .chart-pane { display: flex; flex: 1; min-width: 0; min-height: 0; }
    > .ranking-pane { display: flex; flex: 0 0 auto; min-width: 0; min-height: 0; }
`;
const Divider = styled.div`
    position: relative;
    flex: 0 0 20px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: col-resize;
    touch-action: none;
    user-select: none;
    &::before { content: ''; position: absolute; top: 0; bottom: 0; left: 9px; width: 1px; background: ${(props): string => props.theme.borderColor}; }
    &:hover::before, &[data-dragging='true']::before { width: 2px; background: ${(props): string => props.theme.primaryColor}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: -2px; }
    button { position: relative; display: grid; place-items: center; width: 18px; height: 26px; padding: 0;
        border: 1px solid ${(props): string => props.theme.borderColor}; border-radius: 3px;
        background: ${(props): string => props.theme.bgColorCommon}; color: ${(props): string => props.theme.textColorSecondary}; cursor: pointer; }
    button:hover { color: ${(props): string => props.theme.primaryColor}; }
    button:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; }
    .split-handle { width: 3px; height: 32px; border-radius: 3px; background: ${(props): string => props.theme.bgColorLighter};
        box-shadow: 0 0 0 1px ${(props): string => props.theme.borderColorLighter}; pointer-events: none; }
    &[data-collapsed='right'], &[data-collapsed='left'] {
        position: absolute; top: 0; bottom: 0; width: 20px; z-index: 2;
    }
    &[data-collapsed='right'] { right: 0; }
    &[data-collapsed='left'] { left: 0; }
    &[data-collapsed='right']::before, &[data-collapsed='left']::before { display: none; }
`;
interface Props { chart: React.ReactNode; table: React.ReactNode; visible: boolean; onVisibleChange: (visible: boolean) => void }

export const OverviewSplitPane = ({ chart, table, visible, onVisibleChange }: Props): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const ref = useRef<HTMLDivElement>(null);
    const [containerWidth, setContainerWidth] = useState(1000);
    const [preferredWidth, setPreferredWidth] = useState(310);
    const [chartCollapsed, setChartCollapsed] = useState(false);
    const [dragging, setDragging] = useState(false);
    const drag = useRef<{ x: number; width: number }>();
    const pending = useRef<number>();
    const frame = useRef<number>();
    const available = Math.max(0, containerWidth - 20);
    const width = !visible ? 0 : chartCollapsed ? available : Math.min(Math.max(120, preferredWidth), Math.max(120, available - 160));
    useLayoutEffect(() => {
        const element = ref.current;
        if (!element) return;
        const measure = (): void => {
            const bounds = element.getBoundingClientRect();
            if (bounds.width > 0) setContainerWidth(bounds.width);
        };
        measure();
        if (typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(measure);
        observer.observe(element);
        return () => observer.disconnect();
    }, []);
    useEffect(() => (): void => { if (frame.current !== undefined) cancelAnimationFrame(frame.current); }, []);
    const commit = (): void => {
        frame.current = undefined;
        if (pending.current !== undefined) {
            const next = pending.current;
            if (next <= 120) {
                setChartCollapsed(false);
                onVisibleChange(false);
            } else if (available - next <= 160) {
                setChartCollapsed(true);
                onVisibleChange(true);
            } else {
                setPreferredWidth(next);
                setChartCollapsed(false);
                onVisibleChange(true);
            }
        }
        pending.current = undefined;
    };
    const resize = (value: number): void => {
        pending.current = Math.max(0, Math.min(available, value));
        if (frame.current === undefined) frame.current = requestAnimationFrame(commit);
    };
    const finish = (event: React.PointerEvent<HTMLDivElement>): void => {
        if (frame.current !== undefined) cancelAnimationFrame(frame.current);
        commit();
        drag.current = undefined;
        setDragging(false);
        if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    };
    const restore = (): void => { setChartCollapsed(false); onVisibleChange(true); };
    const control = (label: string, left: boolean, action: () => void): React.ReactElement => (
        <button aria-label={t(label)} title={t(label)}
            onPointerDown={(event): void => event.stopPropagation()} onDoubleClick={(event): void => event.stopPropagation()}
            onClick={action}>
            <svg width="14" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5" aria-hidden="true">
                <path d={left ? 'm10 3-5 5 5 5' : 'm6 3 5 5-5 5'} />
            </svg>
        </button>
    );
    return <Pane ref={ref} data-testid="overviewSplitPane">
        <div className="chart-pane" data-testid="overviewChartPane" style={{ display: visible && chartCollapsed ? 'none' : undefined }}>{chart}</div>
        <Divider data-testid="overviewPanelDivider" role="separator" tabIndex={0}
            aria-label={t('overviewResizePanels')} aria-orientation="vertical" aria-valuemin={0} aria-valuemax={available} aria-valuenow={width}
            data-dragging={dragging} data-collapsed={!visible ? 'right' : chartCollapsed ? 'left' : undefined}
            onPointerDown={(event): void => {
                if (event.button !== 0) return;
                event.preventDefault();
                event.currentTarget.focus();
                drag.current = { x: event.clientX, width };
                event.currentTarget.setPointerCapture(event.pointerId);
                setDragging(true);
            }}
            onPointerMove={(event): void => {
                if (drag.current && event.currentTarget.hasPointerCapture(event.pointerId)) resize(drag.current.width - event.clientX + drag.current.x);
            }} onPointerUp={finish} onPointerCancel={finish}
            onLostPointerCapture={(): void => { drag.current = undefined; setDragging(false); }}
            onDoubleClick={(): void => { setPreferredWidth(310); restore(); }}
            onKeyDown={(event): void => {
                if (event.target !== event.currentTarget) return;
                const next = event.key === 'ArrowLeft'
                    ? !visible ? Math.max(144, preferredWidth) : width + 24
                    : event.key === 'ArrowRight'
                        ? chartCollapsed ? Math.min(available - 184, preferredWidth) : width - 24
                        : event.key === 'Home' ? 0 : event.key === 'End' ? available : undefined;
                if (next !== undefined) { event.preventDefault(); resize(next); }
            }}>
            {!visible
                ? control('overviewShowTable', true, restore)
                : chartCollapsed
                    ? control('overviewShowChart', false, restore)
                    : <span className="split-handle" aria-hidden="true" />}
        </Divider>
        <div className="ranking-pane" data-testid="overviewRankingPane" style={{ width: visible && chartCollapsed ? '100%' : width, display: visible ? undefined : 'none' }}>{table}</div>
    </Pane>;
};
