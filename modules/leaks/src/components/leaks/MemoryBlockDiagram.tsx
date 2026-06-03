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
    GraphShortcutRow,
    GraphShortcutTip,
    GraphShortcutTitle,
    GraphToolbar,
    GraphToolbarTooltipStyle,
    GraphWheelCombo,
    GraphWheelIcon,
    HoverItem,
    Loading,
    MarkLineBlock,
} from './tools';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { Tooltip } from '@insight/lib/components';
import { ColumnWidthOutlined, OneToOneOutlined } from '@ant-design/icons';

const BASE_ZOOM_STEP = 0.1;
const BASE_MOVE_STEP = 5;
const TOOLBAR_HEIGHT = 36;
const BLOCK_DIAGRAM_OFFSET_LEFT = 100;
const BLOCK_DIAGRAM_OFFSET_RIGHT = 105;
const DEFAULT_TRANSFORM: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
type TransformChangeSource = 'wheel' | 'keyboard' | 'drag';

const getTransformScaleX = (transform: RenderOptions['transform']): number => transform.scaleX;
const getTransformScaleY = (transform: RenderOptions['transform']): number => transform.scaleY;

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
    const [xZoomMode, setXZoomMode] = useState(true);
    const xZoomModeRef = useRef(true);
    const isDragging = useRef(false);
    const isClick = useRef(false);
    const dragStartPoint = useRef({ x: 0, y: 0 });

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

        // 获取当前变换参数
        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        const currentScaleX = getTransformScaleX(currentTransform);
        const currentScaleY = getTransformScaleY(currentTransform);
        const onlyScaleX = ev.shiftKey || (!ev.ctrlKey && xZoomModeRef.current);

        // 计算缩放前鼠标在实际内容中的相对位置（相对于画布原点）
        const originalContentMouseX = (mouseX - currentTransform.x) / currentScaleX;
        const originalContentMouseY = (mouseY - currentTransform.y) / currentScaleY;

        // 计算新的缩放值
        const direction = ev.deltaY > 0 ? -1 : 1; // -1: 缩小, +1: 放大

        // 动态步长：离 1 越远，变化越快
        const baseScale = onlyScaleX ? currentScaleX : Math.max(currentScaleX, currentScaleY);
        const distanceFromOne = Math.abs(baseScale - 1) + 1; // 避免为0
        const dynamicStep = BASE_ZOOM_STEP * distanceFromOne;

        let newScaleX = currentScaleX + direction * dynamicStep;
        let newScaleY = onlyScaleX ? currentScaleY : currentScaleY + direction * dynamicStep;

        // 限制最小缩放
        newScaleX = Math.max(0.1, newScaleX);
        newScaleY = Math.max(0.1, newScaleY);

        const maxRangeX = rect.width;
        const minRangeX = -rect.width * newScaleX;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * newScaleY;
        // 计算缩放后的新偏移，使鼠标下的内容位置不变
        // 原始偏移距离 + (内容相对位置 * (新缩放 - 旧缩放))
        const newX = Math.min(Math.max(mouseX - originalContentMouseX * newScaleX, minRangeX), maxRangeX);
        const newY = onlyScaleX
            ? currentTransform.y
            : Math.min(Math.max(mouseY - originalContentMouseY * newScaleY, minRangeY), maxRangeY);

        // 更新变换参数
        const transform = { x: newX, y: newY, scaleX: newScaleX, scaleY: newScaleY };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        onTransformChange?.(transform, 'wheel');
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

    const handleMouseLeave = (): void => {
        ref.current?.blur();
        isDragging.current = false;
        isClick.current = false;
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
        const currentScaleX = getTransformScaleX(currentTransform);
        const currentScaleY = getTransformScaleY(currentTransform);

        const deltaX = currentX - dragStartPoint.current.x;
        const deltaY = currentY - dragStartPoint.current.y;
        const maxRangeX = rect.width;
        const minRangeX = -rect.width * currentScaleX;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * currentScaleY;

        const transform = {
            ...currentTransform,
            x: Math.min(Math.max(currentTransform.x + deltaX, minRangeX), maxRangeX),
            y: Math.min(Math.max(currentTransform.y - deltaY, minRangeY), maxRangeY),
        };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });

        workerTransform({ transform });
        if (deltaX !== 0) {
            onTransformChange?.(transform, 'drag');
        }

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
        if (ref.current === null) {
            return;
        }

        const rect = ref.current.getBoundingClientRect();
        if ((ev.ctrlKey || ev.shiftKey) && ['+', '=', '-', '_'].includes(ev.key)) {
            ev.preventDefault();
            const direction = ev.key === '-' || ev.key === '_' ? -1 : 1;
            const onlyScaleX = ev.shiftKey && !ev.ctrlKey;
            const mouseX = rect.width / 2;
            const mouseY = rect.height / 2;
            const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
            const currentScaleX = getTransformScaleX(currentTransform);
            const currentScaleY = getTransformScaleY(currentTransform);
            const originalContentMouseX = (mouseX - currentTransform.x) / currentScaleX;
            const originalContentMouseY = (mouseY - currentTransform.y) / currentScaleY;
            const baseScale = Math.max(currentScaleX, currentScaleY);
            const dynamicStep = BASE_ZOOM_STEP * (Math.abs(baseScale - 1) + 1);
            const newScaleX = Math.max(0.1, currentScaleX + direction * dynamicStep);
            const newScaleY = onlyScaleX ? currentScaleY : Math.max(0.1, currentScaleY + direction * dynamicStep);
            const maxRangeX = rect.width;
            const minRangeX = -rect.width * newScaleX;
            const maxRangeY = rect.height;
            const minRangeY = -rect.height * newScaleY;
            const transform = {
                x: Math.min(Math.max(mouseX - originalContentMouseX * newScaleX, minRangeX), maxRangeX),
                y: onlyScaleX ? currentTransform.y : Math.min(Math.max(mouseY - originalContentMouseY * newScaleY, minRangeY), maxRangeY),
                scaleX: newScaleX,
                scaleY: newScaleY,
            };
            runInAction(() => {
                session.leaksWorkerInfo.renderOptions.transform = transform;
            });
            workerTransform({ transform });
            onTransformChange?.(transform, 'keyboard');
            return;
        }
        if (ev.key.toLowerCase() === 'r') {
            resetTransform();
            return;
        }
        if (ev.key.toLowerCase() === 'h') {
            toggleXZoomMode();
            return;
        }

        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        const currentScaleX = getTransformScaleX(currentTransform);
        const currentScaleY = getTransformScaleY(currentTransform);
        const maxRangeX = rect.width;
        const minRangeX = -rect.width * currentScaleX;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * currentScaleY;
        let newTransformX = 0;
        let newTransformY = 0;
        switch (ev.key.toLowerCase()) {
            case 'w':
                newTransformY = BASE_MOVE_STEP * currentScaleY;
                break;
            case 's':
                newTransformY = -BASE_MOVE_STEP * currentScaleY;
                break;
            case 'a':
                newTransformX = BASE_MOVE_STEP * currentScaleX;
                break;
            case 'd':
                newTransformX = -BASE_MOVE_STEP * currentScaleX;
                break;
            default:
                return;
        }

        const currentMousePosition = session.markLineInfo.block;

        workerHoverItem({ clientX: currentMousePosition.x, clientY: rect.height - currentMousePosition.y });
        runInAction(() => {
            session.markLineInfo.block = { ...currentMousePosition };
        });
        const transform = {
            ...currentTransform,
            x: Math.min(Math.max(currentTransform.x + newTransformX, minRangeX), maxRangeX),
            y: Math.min(Math.max(currentTransform.y - newTransformY, minRangeY), maxRangeY),
        };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });

        workerTransform({ transform });
        if (newTransformX !== 0) {
            onTransformChange?.(transform, 'keyboard');
        }
    };

    useEffect(() => {
        xZoomModeRef.current = xZoomMode;
    }, [xZoomMode]);

    const renderXZoomTooltip = (): JSX.Element => <GraphShortcutTip>
        <GraphShortcutTitle>{t('enableDisableXZoom')}<GraphShortcutActions><GraphKeycap>H</GraphKeycap></GraphShortcutActions></GraphShortcutTitle>
        <GraphShortcutRow>
            <span>{t('equalZoomHelp')}</span>
            <GraphWheelCombo><GraphKeycap>Ctrl</GraphKeycap><span>+</span><GraphWheelIcon /><span>/</span><GraphKeycap>+</GraphKeycap><GraphKeycap>-</GraphKeycap></GraphWheelCombo>
        </GraphShortcutRow>
        <GraphShortcutRow>
            <span>{t('xZoomWheelHelp')}</span>
            <GraphWheelCombo><GraphKeycap>Shift</GraphKeycap><span>+</span><GraphWheelIcon /><span>/</span><GraphKeycap>+</GraphKeycap><GraphKeycap>-</GraphKeycap></GraphWheelCombo>
        </GraphShortcutRow>
    </GraphShortcutTip>;

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

        canvas.addEventListener('wheel', handleWheel, { passive: false, capture: true });
        canvas.addEventListener('mousedown', handleMouseDown);
        canvas.addEventListener('auxclick', handleMouseDown);
        canvas.addEventListener('mousemove', handleMouseMove);
        canvas.addEventListener('mouseup', handleMouseUp);
        canvas.addEventListener('mouseleave', handleMouseLeave);
        canvas.addEventListener('click', handleClick);
        canvas.addEventListener('keydown', handleKeyDown);

        return () => {
            window.removeEventListener('resize', handleResize);

            canvas.removeEventListener('wheel', handleWheel, { capture: true });
            canvas.removeEventListener('mousedown', handleMouseDown);
            canvas.removeEventListener('auxclick', handleMouseDown);
            canvas.removeEventListener('mousemove', handleMouseMove);
            canvas.removeEventListener('mouseup', handleMouseUp);
            canvas.removeEventListener('mouseleave', handleMouseLeave);
            canvas.removeEventListener('click', handleClick);
            canvas.removeEventListener('keydown', handleKeyDown);
        };
    }, []);

    return <div style={{ width: '100%', height: 530 + TOOLBAR_HEIGHT, boxSizing: 'border-box' }}>
        <GraphToolbarTooltipStyle />
        <div style={{ display: 'flex', justifyContent: 'flex-end', height: TOOLBAR_HEIGHT }}>
            <GraphToolbar style={{ position: 'static' }}>
                <Tooltip title={renderResetTooltip()} placement="topRight" overlayClassName={graphToolbarTooltipClassName} mouseEnterDelay={0} mouseLeaveDelay={0}>
                    <button type="button" aria-label={`${t('resetView')}`} onClick={resetTransform}>
                        <OneToOneOutlined />
                    </button>
                </Tooltip>
                <Tooltip title={renderXZoomTooltip()} placement="topRight" overlayClassName={graphToolbarTooltipClassName} mouseEnterDelay={0} mouseLeaveDelay={0}>
                    <button
                        type="button"
                        className={xZoomMode ? 'active' : undefined}
                        aria-label={xZoomMode ? t('disableXZoom') : t('enableXZoom')}
                        aria-pressed={xZoomMode}
                        onClick={toggleXZoomMode}
                    >
                        <ColumnWidthOutlined />
                    </button>
                </Tooltip>
            </GraphToolbar>
        </div>
        <div
            data-testid="blockDiagramSection"
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
                <Axis session={session} />
                <canvas
                    ref={ref}
                    style={{ imageRendering: 'pixelated', touchAction: 'none', outline: 'none' }}
                />
                <MarkLineBlock session={session} />
                <HoverItem session={session} />
                <Loading style={{ position: 'absolute', top: 0, left: 0, width: '100%', height: '100%' }} loading={session.loadingBlocks} />
            </div>
        </div>
    </div>;
});
