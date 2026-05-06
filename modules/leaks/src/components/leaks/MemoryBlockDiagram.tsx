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

import React, { useEffect, useRef } from 'react';
import {
    workerInitCanvas,
    workerResizeCanvas,
    workerTransform,
    workerHoverItem,
    workerClickItem,
} from '@/leaksWorker/blockWorker/worker';
import { workerSelectItem as workerSelectStateItem } from '@/leaksWorker/stateWorker/worker';
import { Session } from '@/entity/session';
import { runInAction } from 'mobx';
import { Axis, HoverItem, Loading, MarkLineBlock } from './tools';
import { observer } from 'mobx-react';

const BASE_ZOOM_STEP = 0.1;
const BASE_MOVE_STEP = 5;

export const MemoryBlockDiagram = observer(({ session }: { session: Session }): JSX.Element => {
    const containerRef = useRef<HTMLDivElement>(null);
    const ref = useRef<HTMLCanvasElement>(null);
    const isDragging = useRef(false);
    const isClick = useRef(false);
    const dragStartPoint = useRef({ x: 0, y: 0 });

    const handleResize = (): void => {
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const containerRect = containerRef.current.getBoundingClientRect();
        const width = containerRect.width - 100;
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

        // 计算缩放前鼠标在实际内容中的相对位置（相对于画布原点）
        const originalContentMouseX = (mouseX - currentTransform.x) / currentTransform.scale;
        const originalContentMouseY = (mouseY - currentTransform.y) / currentTransform.scale;

        // 计算新的缩放值
        const direction = ev.deltaY > 0 ? -1 : 1; // -1: 缩小, +1: 放大

        // 动态步长：离 1 越远，变化越快
        const distanceFromOne = Math.abs(currentTransform.scale - 1) + 1; // 避免为0
        const dynamicStep = BASE_ZOOM_STEP * distanceFromOne;

        let newScale = currentTransform.scale + direction * dynamicStep;

        // 限制最小缩放
        newScale = Math.max(0.1, newScale);

        const maxRangeX = rect.width;
        const minRangeX = -rect.width * newScale;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * newScale;
        // 计算缩放后的新偏移，使鼠标下的内容位置不变
        // 原始偏移距离 + (内容相对位置 * (新缩放 - 旧缩放))
        const newX = Math.min(Math.max(mouseX - originalContentMouseX * newScale, minRangeX), maxRangeX);
        const newY = Math.min(Math.max(mouseY - originalContentMouseY * newScale, minRangeY), maxRangeY);

        // 更新变换参数
        const transform = { x: newX, y: newY, scale: newScale };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
    };

    const handleMouseDown = (ev: MouseEvent): void => {
        if (ev.button !== 0 || ref.current === null) {
            return;
        }
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

        const deltaX = currentX - dragStartPoint.current.x;
        const deltaY = currentY - dragStartPoint.current.y;
        const maxRangeX = rect.width;
        const minRangeX = -rect.width * currentTransform.scale;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * currentTransform.scale;

        const transform = {
            ...currentTransform,
            x: Math.min(Math.max(currentTransform.x + deltaX, minRangeX), maxRangeX),
            y: Math.min(Math.max(currentTransform.y - deltaY, minRangeY), maxRangeY),
        };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });

        workerTransform({ transform });

        dragStartPoint.current = { x: currentX, y: currentY };
    };

    const handleClick = (ev: MouseEvent): void => {
        if (ref.current === null) {
            return;
        }
        if (isClick.current) {
            isClick.current = false;
            const rect = ref.current.getBoundingClientRect();
            workerSelectStateItem({ item: null });
            runInAction(() => {
                session.stateWorkerInfo.clickItem = null;
                session.clickEventItem = null;
            });
            workerClickItem({ clientX: ev.clientX - rect.left, clientY: rect.height - (ev.clientY - rect.top) });
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
        const currentTransform = session.leaksWorkerInfo.renderOptions.transform;
        const maxRangeX = rect.width;
        const minRangeX = -rect.width * currentTransform.scale;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * currentTransform.scale;
        let newTransformX = 0;
        let newTransformY = 0;
        switch (ev.key.toLowerCase()) {
            case 'w':
                newTransformY = BASE_MOVE_STEP * currentTransform.scale;
                break;
            case 's':
                newTransformY = -BASE_MOVE_STEP * currentTransform.scale;
                break;
            case 'a':
                newTransformX = BASE_MOVE_STEP * currentTransform.scale;
                break;
            case 'd':
                newTransformX = -BASE_MOVE_STEP * currentTransform.scale;
                break;
            default:
                break;
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
    };

    useEffect(() => {
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const canvas = ref.current;
        try {
            const containerRect = containerRef.current.getBoundingClientRect();
            const width = containerRect.width - 100;
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
        canvas.addEventListener('mousemove', handleMouseMove);
        canvas.addEventListener('mouseup', handleMouseUp);
        canvas.addEventListener('mouseleave', handleMouseLeave);
        canvas.addEventListener('click', handleClick);
        canvas.addEventListener('keydown', handleKeyDown);

        return () => {
            window.removeEventListener('resize', handleResize);

            canvas.removeEventListener('wheel', handleWheel, { capture: true });
            canvas.removeEventListener('mousedown', handleMouseDown);
            canvas.removeEventListener('mousemove', handleMouseMove);
            canvas.removeEventListener('mouseup', handleMouseUp);
            canvas.removeEventListener('mouseleave', handleMouseLeave);
            canvas.removeEventListener('click', handleClick);
            canvas.removeEventListener('keydown', handleKeyDown);
        };
    }, []);

    return <div ref={containerRef} style={{ width: 'calc(100% - 100px)', height: 500, paddingLeft: 100, paddingTop: 20 }}>
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
    </div>;
});
