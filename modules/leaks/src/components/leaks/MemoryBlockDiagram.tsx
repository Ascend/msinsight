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

import React, { useEffect, useRef, useState } from 'react';
import {
    workerInitCanvas,
    workerResizeCanvas,
    workerTransform,
    workerHoverItem,
    workerClickItem,
    workerSelectBlockById,
} from '@/leaksWorker/blockWorker/worker';
import { workerSelectItem as workerSelectStateItem } from '@/leaksWorker/stateWorker/worker';
import { Session } from '@/entity/session';
import { runInAction } from 'mobx';
import {
    Axis,
    graphToolbarTooltipClassName,
    GraphKeycap,
    GraphMouseIcon,
    GraphShortcutActions,
    GraphShortcutTip,
    GraphShortcutTitle,
    GraphToolbar,
    GraphToolbarTooltipStyle,
    HoverItem,
    Loading,
    MarkLineBlock,
} from './tools';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { Spin } from '@insight/lib';
import { Tooltip } from '@insight/lib/components';
import { OneToOneOutlined } from '@ant-design/icons';
import {
    calculateLifecyclePanTransform,
    calculateLifecycleZoomTransform,
    isEditableKeyboardTarget,
    isLifecycleHostZoomShortcut,
    resolveLifecycleKeyboardAction,
} from './lifecycleNavigation';
import { LifecycleMemoryMarkerOverlay } from './LifecycleMemoryMarkerOverlay';
import {
    LifecycleGraphToolbar,
    LifecycleZoomModeIcon,
    LifecycleZoomModeTooltip,
    type LifecycleZoomMode,
} from './LifecycleGraphToolbar';

const BASE_MOVE_STEP = 5;
const TOOLBAR_HEIGHT = 36;
const BLOCK_DIAGRAM_OFFSET_LEFT = 100;
const BLOCK_DIAGRAM_OFFSET_RIGHT = 105;
const DEFAULT_TRANSFORM: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
type TransformChangeSource = 'wheel' | 'keyboard' | 'drag';

const ProgressiveLoadingStatus = styled.div`
    position: absolute;
    top: 8px;
    right: 48px;
    z-index: 2;
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 6px;
    min-width: 72px;
    min-height: 28px;
    padding: 4px 8px;
    box-sizing: border-box;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: ${(props): string => props.theme.boxShadow};
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    font-variant-numeric: tabular-nums;
    line-height: 18px;
    pointer-events: none;
    user-select: none;
`;

export const MemoryBlockDiagram = observer(({
    session,
    onResetTransform,
    onTransformChange,
}: {
    session: Session;
    onResetTransform?: () => void;
    onTransformChange?: (transform: RenderOptions['transform'], source?: TransformChangeSource) => void;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const containerRef = useRef<HTMLDivElement>(null);
    const ref = useRef<HTMLCanvasElement>(null);
    const defaultXZoomMode = false;
    const [xZoomMode, setXZoomMode] = useState(defaultXZoomMode);
    const xZoomModeRef = useRef(defaultXZoomMode);
    const isDragging = useRef(false);
    const isClick = useRef(false);
    const dragStartPoint = useRef({ x: 0, y: 0 });
    const lastPointerPosition = useRef<{ x: number; y: number } | null>(null);
    const markerIdSequence = useRef(0);
    const plotInteractionActive = useRef(false);
    const progressiveRenderPercent = session.progressiveTotalEventCount > 0
        ? Math.min(session.loadingBlocks ? 99 : 100, Math.floor(
            session.progressiveRenderedEventCount * 100 / session.progressiveTotalEventCount,
        ))
        : 0;
    const createMemoryMarker = (memoryBytes: number): void => {
        const { minSize, maxSize } = session.leaksWorkerInfo.sizeInfo;
        if (
            !Number.isFinite(memoryBytes) ||
            memoryBytes < minSize ||
            memoryBytes > maxSize
        ) {
            return;
        }
        markerIdSequence.current += 1;
        session.addLifecycleMemoryMarker(memoryBytes, `memory-marker-${Date.now()}-${markerIdSequence.current}`);
    };

    const resetTransform = (): void => {
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = { ...DEFAULT_TRANSFORM };
            session.markLineInfo.block = { x: -1, y: -1 };
            session.markLineInfo.stack = { x: -1, y: -1 };
        });
        workerTransform({ transform: { ...DEFAULT_TRANSFORM } });
        workerHoverItem({ clientX: -1, clientY: -1 });
        onResetTransform?.();
    };

    const toggleXZoomMode = (): void => {
        setXZoomMode(mode => {
            const nextMode = !mode;
            xZoomModeRef.current = nextMode;
            return nextMode;
        });
    };

    const setZoomMode = (mode: LifecycleZoomMode): void => {
        const horizontal = mode === 'horizontal';
        xZoomModeRef.current = horizontal;
        setXZoomMode(horizontal);
    };

    const applyTransform = (transform: RenderOptions['transform'], source: TransformChangeSource): void => {
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        onTransformChange?.(transform, source);
    };

    const handleResize = (): void => {
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const containerRect = containerRef.current.getBoundingClientRect();
        const width = containerRect.width - BLOCK_DIAGRAM_OFFSET_LEFT - BLOCK_DIAGRAM_OFFSET_RIGHT;
        const height = containerRect.height - 50;
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.viewport = { width, height };
        });
        workerResizeCanvas({ width, height });
    };

    const handleWheel = (ev: WheelEvent): void => {
        ev.preventDefault();

        if (ref.current === null) {
            return;
        }

        const rect = ref.current.getBoundingClientRect();

        // 计算鼠标相对于画布的坐标
        const mouseX = ev.clientX - rect.left;
        const mouseY = rect.height - (ev.clientY - rect.top);

        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        const onlyScaleX = ev.shiftKey || (!ev.ctrlKey && xZoomModeRef.current);
        const direction = ev.deltaY > 0 ? -1 : 1; // -1: 缩小, +1: 放大
        const transform = calculateLifecycleZoomTransform({
            transform: currentTransform,
            viewport: { width: rect.width, height: rect.height },
            anchorX: mouseX,
            anchorY: mouseY,
            direction,
            onlyScaleX,
        });
        lastPointerPosition.current = { x: mouseX, y: rect.height - mouseY };
        applyTransform(transform, 'wheel');
    };

    const handleMouseDown = (ev: MouseEvent): void => {
        if (ref.current === null) {
            return;
        }
        if (ev.button === 1) {
            ev.preventDefault();
            ref.current.focus({ preventScroll: true });
            resetTransform();
            return;
        }
        if (ev.button !== 0) {
            return;
        }
        ref.current.focus({ preventScroll: true });
        isClick.current = true;
        const rect = ref.current.getBoundingClientRect();
        dragStartPoint.current = {
            x: ev.clientX - rect.left,
            y: ev.clientY - rect.top,
        };
    };

    const handleMouseUp = (): void => {
        isDragging.current = false;
    };

    const handleCanvasFocus = (): void => {
        plotInteractionActive.current = true;
    };

    const handleCanvasBlur = (): void => {
        plotInteractionActive.current = false;
    };

    const handleCanvasMouseEnter = (): void => {
        plotInteractionActive.current = true;
    };

    const handleMouseLeave = (): void => {
        ref.current?.blur();
        plotInteractionActive.current = false;
        isDragging.current = false;
        isClick.current = false;
        lastPointerPosition.current = null;
        runInAction(() => {
            session.markLineInfo.block = { x: -1, y: -1 };
            session.markLineInfo.stack = { x: -1, y: -1 };
        });
        workerHoverItem({ clientX: -1, clientY: -1 });
    };

    const handleMouseMove = (ev: MouseEvent): void => {
        if (ref.current === null) {
            return;
        }
        ref.current.focus({ preventScroll: true });
        const rect = ref.current.getBoundingClientRect();
        const currentX = ev.clientX - rect.left;
        const currentY = ev.clientY - rect.top;
        lastPointerPosition.current = { x: currentX, y: currentY };
        plotInteractionActive.current = true;

        if (isClick.current) {
            const moved = Math.abs(currentX - dragStartPoint.current.x) > 1 ||
                          Math.abs(currentY - dragStartPoint.current.y) > 1;
            if (moved) {
                isClick.current = false;
                isDragging.current = true;
            }
        }
        if (!isDragging.current) {
            workerHoverItem({ clientX: currentX, clientY: rect.height - currentY });
            runInAction(() => {
                session.markLineInfo.block = { x: currentX, y: currentY };
            });
            return;
        }
        runInAction(() => {
            session.markLineInfo.block = { x: -1, y: -1 };
        });

        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        const deltaX = currentX - dragStartPoint.current.x;
        const deltaY = currentY - dragStartPoint.current.y;
        const transform = calculateLifecyclePanTransform(
            currentTransform,
            { width: rect.width, height: rect.height },
            deltaX,
            -deltaY,
        );
        applyTransform(transform, 'drag');

        dragStartPoint.current = { x: currentX, y: currentY };
    };

    const handleClick = (ev: MouseEvent): void => {
        if (ref.current === null) {
            return;
        }
        if (isClick.current) {
            isClick.current = false;
            const rect = ref.current.getBoundingClientRect();
            const selectionVersion = session.selectionVersion + 1;
            workerSelectStateItem({ item: null, selectionVersion });
            runInAction(() => {
                session.selectionVersion = selectionVersion;
                session.stateWorkerInfo.clickItem = null;
                session.clickEventItem = null;
            });
            workerClickItem({ clientX: ev.clientX - rect.left, clientY: rect.height - (ev.clientY - rect.top), selectionVersion });
            if (session.markLineInfo.block.x > -1) {
                runInAction(() => {
                    session.memoryStamp = Math.round(session.markLineInfo.currentTimestamp);
                });
            }
        }
    };

    const handleKeyDown = (ev: KeyboardEvent): void => {
        if (ref.current === null || isEditableKeyboardTarget(ev.target) || ev.isComposing) {
            return;
        }

        const rect = ref.current.getBoundingClientRect();
        if ((ev.ctrlKey || ev.shiftKey) && ['+', '=', '-', '_'].includes(ev.key)) {
            ev.preventDefault();
            const direction = ev.key === '-' || ev.key === '_' ? -1 : 1;
            const onlyScaleX = ev.shiftKey && !ev.ctrlKey;
            const pointer = lastPointerPosition.current;
            const mouseX = pointer?.x ?? rect.width / 2;
            const mouseY = pointer === null ? rect.height / 2 : rect.height - pointer.y;
            const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
            const transform = calculateLifecycleZoomTransform({
                transform: currentTransform,
                viewport: { width: rect.width, height: rect.height },
                anchorX: mouseX,
                anchorY: mouseY,
                direction,
                onlyScaleX,
            });
            applyTransform(transform, 'keyboard');
            return;
        }

        const hasModifier = ev.ctrlKey || ev.metaKey || ev.altKey || ev.shiftKey;
        if (!hasModifier && ev.key.toLowerCase() === 'r') {
            ev.preventDefault();
            resetTransform();
            return;
        }
        if (!hasModifier && ev.key.toLowerCase() === 'h') {
            ev.preventDefault();
            toggleXZoomMode();
            return;
        }

        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        let newTransformX = 0;
        let newTransformY = 0;
        const action = resolveLifecycleKeyboardAction(ev);
        if (action === 'zoom-x-in' || action === 'zoom-x-out' || action === 'zoom-all-in' || action === 'zoom-all-out') {
            ev.preventDefault();
            const pointer = lastPointerPosition.current;
            const transform = calculateLifecycleZoomTransform({
                transform: currentTransform,
                viewport: { width: rect.width, height: rect.height },
                anchorX: pointer?.x ?? rect.width / 2,
                anchorY: pointer === null ? rect.height / 2 : rect.height - pointer.y,
                direction: action === 'zoom-x-in' || action === 'zoom-all-in' ? 1 : -1,
                onlyScaleX: action === 'zoom-x-in' || action === 'zoom-x-out',
            });
            applyTransform(transform, 'keyboard');
            return;
        }

        if (action !== null) {
            ev.preventDefault();
            if (action === 'pan-left') newTransformX = BASE_MOVE_STEP * currentTransform.scaleX;
            if (action === 'pan-right') newTransformX = -BASE_MOVE_STEP * currentTransform.scaleX;
            if (action === 'pan-up') newTransformY = -BASE_MOVE_STEP * currentTransform.scaleY;
            if (action === 'pan-down') newTransformY = BASE_MOVE_STEP * currentTransform.scaleY;
        } else {
            return;
        }

        const currentMousePosition = session.markLineInfo.block;
        workerHoverItem({ clientX: currentMousePosition.x, clientY: rect.height - currentMousePosition.y });
        runInAction(() => {
            session.markLineInfo.block = { ...currentMousePosition };
        });
        const transform = calculateLifecyclePanTransform(
            currentTransform,
            { width: rect.width, height: rect.height },
            newTransformX,
            newTransformY,
        );
        applyTransform(transform, 'keyboard');
    };

    const handleHostZoomShortcut = (ev: KeyboardEvent): void => {
        if (
            !isLifecycleHostZoomShortcut(ev, plotInteractionActive.current) ||
            isEditableKeyboardTarget(ev.target) ||
            ev.isComposing
        ) {
            return;
        }
        ev.preventDefault();
        ev.stopPropagation();
        ev.stopImmediatePropagation();
        handleKeyDown(ev);
    };

    useEffect(() => {
        xZoomModeRef.current = xZoomMode;
    }, [xZoomMode]);

    const renderResetTooltip = (): JSX.Element => <GraphShortcutTip>
        <GraphShortcutTitle>
            {t('resetView')}
            <GraphShortcutActions><GraphKeycap>R</GraphKeycap><span>/</span><GraphMouseIcon /></GraphShortcutActions>
        </GraphShortcutTitle>
    </GraphShortcutTip>;

    useEffect(() => {
        const targetBlockId = session.pendingBlockLocateId;
        if (targetBlockId === null) {
            return;
        }
        document.querySelector('[data-testid="blockDiagramPanel"]')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
        const selectionVersion = session.selectionVersion + 1;
        workerSelectStateItem({ item: null, selectionVersion });
        runInAction(() => {
            session.selectionVersion = selectionVersion;
            session.stateWorkerInfo.clickItem = null;
            session.clickEventItem = null;
            session.pendingBlockLocateId = null;
        });
        workerSelectBlockById({ blockId: targetBlockId, selectionVersion });
    }, [session.pendingBlockLocateId]);

    useEffect(() => {
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const canvas = ref.current;
        try {
            const containerRect = containerRef.current.getBoundingClientRect();
            const width = containerRect.width - BLOCK_DIAGRAM_OFFSET_LEFT - BLOCK_DIAGRAM_OFFSET_RIGHT;
            const height = containerRect.height - 50;

            runInAction(() => {
                session.leaksWorkerInfo.renderOptions.viewport = { width, height };
            });
            workerInitCanvas({ canvas, width, height });
        } catch (_e) {
            // 进入这里，说明画布已经离屏代理，不需要做额外处理
        }
        handleResize();
    }, []);

    useEffect(() => {
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const canvas = ref.current;
        canvas.tabIndex = 0;

        window.addEventListener('resize', handleResize);
        window.addEventListener('blur', handleMouseLeave);
        window.addEventListener('keydown', handleHostZoomShortcut, true);

        canvas.addEventListener('wheel', handleWheel, { passive: false, capture: true });
        canvas.addEventListener('mousedown', handleMouseDown);
        canvas.addEventListener('auxclick', handleMouseDown);
        canvas.addEventListener('mousemove', handleMouseMove);
        canvas.addEventListener('mouseenter', handleCanvasMouseEnter);
        canvas.addEventListener('mouseup', handleMouseUp);
        canvas.addEventListener('mouseleave', handleMouseLeave);
        canvas.addEventListener('focus', handleCanvasFocus);
        canvas.addEventListener('blur', handleCanvasBlur);
        canvas.addEventListener('click', handleClick);
        canvas.addEventListener('keydown', handleKeyDown);

        return () => {
            window.removeEventListener('resize', handleResize);
            window.removeEventListener('blur', handleMouseLeave);
            window.removeEventListener('keydown', handleHostZoomShortcut, true);

            canvas.removeEventListener('wheel', handleWheel, { capture: true });
            canvas.removeEventListener('mousedown', handleMouseDown);
            canvas.removeEventListener('auxclick', handleMouseDown);
            canvas.removeEventListener('mousemove', handleMouseMove);
            canvas.removeEventListener('mouseenter', handleCanvasMouseEnter);
            canvas.removeEventListener('mouseup', handleMouseUp);
            canvas.removeEventListener('mouseleave', handleMouseLeave);
            canvas.removeEventListener('focus', handleCanvasFocus);
            canvas.removeEventListener('blur', handleCanvasBlur);
            canvas.removeEventListener('click', handleClick);
            canvas.removeEventListener('keydown', handleKeyDown);
        };
    }, []);

    const legacyToolbarHeight = session.module === 'memsnapshot' ? 0 : TOOLBAR_HEIGHT;

    return <div style={{ width: '100%', height: 530 + legacyToolbarHeight, boxSizing: 'border-box' }}>
        {session.module !== 'memsnapshot'
            ? <>
                <GraphToolbarTooltipStyle />
                <div style={{ display: 'flex', justifyContent: 'flex-end', height: legacyToolbarHeight }}>
                    <GraphToolbar style={{ position: 'static' }}>
                        <Tooltip title={renderResetTooltip()} placement="topRight" overlayClassName={graphToolbarTooltipClassName} mouseEnterDelay={0} mouseLeaveDelay={0}>
                            <button type="button" aria-label={`${t('resetView')}`} onClick={resetTransform}>
                                <OneToOneOutlined />
                            </button>
                        </Tooltip>
                        <Tooltip title={<LifecycleZoomModeTooltip zoomMode={xZoomMode ? 'horizontal' : 'proportional'} />} placement="topRight" overlayClassName={graphToolbarTooltipClassName} mouseEnterDelay={0} mouseLeaveDelay={0}>
                            <button
                                type="button"
                                aria-label={xZoomMode ? t('switchToProportionalZoom') : t('switchToHorizontalZoom')}
                                onClick={toggleXZoomMode}
                            >
                                <LifecycleZoomModeIcon zoomMode={xZoomMode ? 'horizontal' : 'proportional'} />
                            </button>
                        </Tooltip>
                    </GraphToolbar>
                </div>
            </>
            : <></>}
        <div
            data-testid="blockDiagramSection"
            data-loading-blocks={String(session.loadingBlocks)}
            data-loading-overview={String(session.loadingOverview)}
            data-blocking-spinner-visible={String(session.loadingBlocks && !session.progressiveBlocksVisible)}
            data-progressive-loading-visible={String(session.loadingBlocks && session.progressiveBlocksVisible)}
            data-graph-max-size={String(session.leaksWorkerInfo.sizeInfo.maxSize)}
            data-progressive-visible={String(session.progressiveBlocksVisible)}
            data-progressive-batch-count={String(session.progressiveRenderedBatchCount)}
            data-progressive-instance-count={String(session.progressiveRenderedInstanceCount)}
            data-progressive-rendered-event-count={String(session.progressiveRenderedEventCount)}
            data-progressive-total-event-count={String(session.progressiveTotalEventCount)}
            data-progressive-render-percent={String(progressiveRenderPercent)}
            data-progressive-first-batch-count={String(session.progressiveFirstRenderedBatchCount)}
            data-progressive-first-instance-count={String(session.progressiveFirstRenderedInstanceCount)}
            ref={containerRef}
            style={{
                width: '100%',
                height: 530,
                paddingLeft: BLOCK_DIAGRAM_OFFSET_LEFT,
                paddingRight: BLOCK_DIAGRAM_OFFSET_RIGHT,
                paddingTop: 20,
                boxSizing: 'border-box',
            }}
        >
            <div style={{ position: 'relative' }}>
                {session.module === 'memsnapshot'
                    ? <LifecycleGraphToolbar
                        zoomMode={xZoomMode ? 'horizontal' : 'proportional'}
                        onZoomModeChange={setZoomMode}
                        onReset={resetTransform}
                    />
                    : <></>}
                <Axis session={session} />
                <canvas
                    ref={ref}
                    style={{ imageRendering: 'pixelated', touchAction: 'none', outline: 'none' }}
                />
                <MarkLineBlock session={session} />
                {session.module === 'memsnapshot'
                    ? <LifecycleMemoryMarkerOverlay
                        session={session}
                        onCreateMarker={createMemoryMarker}
                    />
                    : <></>}
                <HoverItem session={session} />
                {session.loadingBlocks && session.progressiveBlocksVisible
                    ? <ProgressiveLoadingStatus
                        data-testid="progressiveBlockLoading"
                        role="progressbar"
                        aria-label={t('parsing')}
                        aria-valuemin={0}
                        aria-valuemax={100}
                        aria-valuenow={progressiveRenderPercent}
                    >
                        <Spin size="small" />
                        <span>{`${progressiveRenderPercent}%`}</span>
                    </ProgressiveLoadingStatus>
                    : <></>}
                <Loading
                    style={{
                        position: 'absolute',
                        top: 0,
                        left: 0,
                        width: '100%',
                        height: '100%',
                        background: session.progressiveBlocksVisible ? 'transparent' : undefined,
                    }}
                    loading={session.loadingBlocks && !session.progressiveBlocksVisible}
                />
            </div>
        </div>
    </div>;
});
