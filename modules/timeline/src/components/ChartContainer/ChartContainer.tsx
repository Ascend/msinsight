/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import type { Theme } from '@emotion/react';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import React, { useEffect, useMemo, useRef } from 'react';
import { observable, runInAction } from 'mobx';
import { Resizor } from '@insight/lib/resize';
// hooks
import { useWatchResize } from '../../utils/useWatchDomResize';
// support utils/types
import type { Session } from '../../entity/session';
// components
import { ChartRow, type ChartRowProps } from '../base/ChartRow';
import { ChartInteractor } from '../charts/ChartInteractor';
import { ContextMenu } from '../ContextMenu';
// same level infer
import { PinnedUnits } from './PinnedUnits';
import { RefUnits } from './Units';
// common constant variable
import ChartHeader from './ChartHeader';
import HorizontalScroller from './HorizontalScrollbar';
import type { ChartInteractorHandles, InteractorMouseState } from '../charts/ChartInteractor/ChartInteractor';
import type { ExtendPos, Pos } from '../charts/ChartInteractor/common';
import { THUMB_WIDTH_PX } from '../base';
import { MouseDownActionResult } from '../charts/ChartInteractor/actions';
import { loopActionFactory } from '../../utils/FactoryActions';
import { RenderEngineContext } from '../../context/context';
import { renderEngine } from '../../renderEngine';
import { DragDirection, useDraggableContainerEx } from '../../utils/useDraggableContainerEx';
import { ActionManager } from '../../actions/manager';
import KeyInfoTooltip from './KeyInfoTooltip';
import FloatingToolbar from './FloatingToolbar';
import { isMac } from '../../utils/is';

const DEFAULT_LANE_INFO_WIDTH = 250;
const DEFAULT_LANE_CHART_WIDTH = 100;
export const TIME_LINE_AXIS_HEIGHT_PX = 30;
const LANE_INFO_WIDTH_PX = observable({ value: DEFAULT_LANE_INFO_WIDTH });
export const CHARTINTERACTOR_NAME = 'chartInteractor';

const Container = styled.div`
    flex-grow: 1;
    height: 100%;
    overflow: hidden;
    position: relative;
    display: flex;
    flex-direction: column;
    background-color: ${(props): string => props.theme.bgColor};
    border-radius: ${(props): string => props.theme.borderRadiusBase};

    .mask {
        position: absolute;
    }

    /* container header - timeline axis area */
    .timeStamp {
        display: flex;
        background-color: ${(props): string => props.theme.contentBackgroundColor};
        height: ${TIME_LINE_AXIS_HEIGHT_PX}px;
    }

    .line {
        position: absolute;
        bottom: 0;
        width: 100%;
        height: 1px;
    }
`;
interface Props {
    session: Session;
    actionManager: ActionManager;
    interactive?: boolean;
    theme?: Theme;
}
// Overlay is not cover the lane info area, so it has a left offset -> LANE_INFO_WIDTH_PX
export const Overlay = styled(({ leftOffset, ...props }: { leftOffset: number } & Omit<ChartRowProps, 'leftWidth'>) => (<ChartRow leftWidth={leftOffset} {...props} />))`
    width: 100%;
    height: 100%;
    position: absolute;
    top:0;
    left:0;
    right:0;
    bottom:0;
    pointer-events: none;
`;

interface ChartBodyProps {
    session: Session;
    interactive?: boolean;
    interactorMouseState: InteractorMouseState;
    chartInteractorRef: React.RefObject<ChartInteractorHandles>;
    mKeyRange: number[];
}

const ChartBody = observer((props: ChartBodyProps) => {
    const { session, interactive, interactorMouseState, chartInteractorRef } = props;
    const { domainStart, domainEnd } = session.domainRange;
    const [height, ref] = useWatchResize<HTMLDivElement>('height');
    const [pinnedHeight, pinnedRef] = useWatchResize<HTMLDivElement>('height');
    const splitLineRef = React.useRef<HTMLDivElement>(null);
    const [view, handleSwitchOpen] = useDraggableContainerEx(
        { draggableWH: 100, dragDirection: DragDirection.TOP, splitLineRef, open: session.pinnedUnits.length > 0, session });

    useEffect(() => {
        if (session.pinnedUnits.length > 0) {
            handleSwitchOpen(true);
        } else {
            handleSwitchOpen(false);
        }
        runInAction(() => {
            session.renderTrigger = !session.renderTrigger;
        });
    }, [session.pinnedUnits]);
    // 监听 selectedUnitKeys 变化表示选中泳道变化，需要重绘
    useEffect(() => {
        runInAction(() => {
            session.renderTrigger = !session.renderTrigger;
        });
    }, [session.selectedUnitKeys]);
    return (<>
        {
            view({
                mainContainer: <RefUnits session={session} height={height} ref={ref}
                    hasPinButton={Boolean(interactive)} laneInfoWidth={LANE_INFO_WIDTH_PX.value} />,
                draggableContainer: <PinnedUnits session={session} height={pinnedHeight} ref={pinnedRef} laneInfoWidth={LANE_INFO_WIDTH_PX.value} />,
                id: 'UnitsPage',
                gap: true,
            })
        }
        <Overlay leftOffset={LANE_INFO_WIDTH_PX.value} rightAreaName={CHARTINTERACTOR_NAME}>
            <><Resizor style={{ width: '7px', right: '1px', pointerEvents: 'all' }} onResize={(deltaX: number, width: number, nextWidth?: number): void => {
                runInAction(() => {
                    if (width > DEFAULT_LANE_INFO_WIDTH && nextWidth != null && nextWidth > DEFAULT_LANE_CHART_WIDTH) {
                        LANE_INFO_WIDTH_PX.value = width;
                    }
                });
            }}/> </>
            <ChartInteractor ref={chartInteractorRef} splitLineRef={splitLineRef} domainStart={domainStart}
                domainEnd={domainEnd} endTimeAll={session.endTimeAll}
                interactorMouseState={interactorMouseState} isNsMode={session.isNsMode} session={session}
                renderTrigger={session.renderTrigger} scrollTop={session.scrollTop} selectedRange={session.selectedRange}
            />
        </Overlay>
        <ContextMenu session={session} interactorMouseState={interactorMouseState} chartInteractorRef={chartInteractorRef} />
    </>);
});

export const ChartContainer = observer((props: Props) => {
    const { session, actionManager } = props;
    const [containerDom, setContainerDom] = React.useState<HTMLDivElement | undefined>(undefined);
    const chartInteractorRef = useRef<ChartInteractorHandles>(null);
    const scrollerRef = React.useRef<HTMLDivElement>(null);
    const { onMouseUp, onKeyDown, interactorMouseState, ...otherInteractors } =
        useInteractorMouseState(chartInteractorRef, scrollerRef, session, !!props.interactive);
    useEffect(() => {
        if (containerDom === undefined) {
            return (): void => {};
        }
        document.addEventListener('mouseup', onMouseUp);
        return (): void => {
            document.removeEventListener('mouseup', onMouseUp);
        };
    }, [containerDom]);
    const keyHoldAction = useMemo(() => loopActionFactory(
        (e: React.KeyboardEvent<HTMLDivElement>) => actionManager.handleKeyDown(e, interactorMouseState, chartInteractorRef.current?.xScale), 16, 50),
    [session]);
    const setPanModifierState = (pressed: boolean): void => {
        session.panModePressed = pressed;
    };
    const handleKeyDownEvent = (e: KeyboardEvent): void => {
        if (isPanModifierPressed(e)) {
            setPanModifierState(true);
        }
        if (!e.repeat) {
            keyHoldAction.clearLoop();
            keyHoldAction.beginLoop(e as unknown as React.KeyboardEvent<HTMLDivElement>);
        }
    };
    const handleKeyUpEvent = (e: KeyboardEvent): void => {
        if (!isPanModifierPressed(e)) {
            setPanModifierState(false);
        }
        keyHoldAction.clearLoop();
        requestAnimationFrame(() => { actionManager.handleKeyUp(e); });
    };

    const handleBlurEvent = (): void => {
        keyHoldAction.clearLoop();
        setPanModifierState(false);
    };

    useEffect(() => {
        document.addEventListener('keydown', handleKeyDownEvent);
        document.addEventListener('keyup', handleKeyUpEvent);
        window.addEventListener('blur', handleBlurEvent);

        return (): void => {
            setPanModifierState(false);
            document.removeEventListener('keydown', handleKeyDownEvent);
            document.removeEventListener('keyup', handleKeyUpEvent);
            window.removeEventListener('blur', handleBlurEvent);
        };
    }, []);
    return <Container
        {...otherInteractors}
        ref={(dom): void => {
            setContainerDom(dom ?? undefined);
        }}
        tabIndex={-1}
        id={'main-container'}
    >
        <RenderEngineContext.Provider value={renderEngine}>
            <ChartHeader
                session={session}
                laneInfoWidth={LANE_INFO_WIDTH_PX.value}
                timelineHeight={TIME_LINE_AXIS_HEIGHT_PX}
                showRecommendation={!props.interactive}
            />
            <ChartBody session={session} interactive={props.interactive} interactorMouseState={interactorMouseState}
                chartInteractorRef={chartInteractorRef} mKeyRange = {session.mMaskRange}/>
        </RenderEngineContext.Provider>
        <HorizontalScroller
            session={session}
            leftLaneInfoWidth={LANE_INFO_WIDTH_PX.value}
            containerDom={containerDom}
            scrollerRef={scrollerRef}
        />
        <FloatingToolbar session={session} />
        <KeyInfoTooltip session={session} />
    </Container>;
});

const isMouseOnScrollbar = (e: React.MouseEvent, horizontalScroller: HTMLElement | null): boolean => {
    const target = e.target as HTMLElement;
    const maxClientX = target.getBoundingClientRect().right;
    return (e.clientX >= maxClientX - THUMB_WIDTH_PX) || (target === horizontalScroller);
};

function isTargetElement(event: React.MouseEvent): boolean {
    let ele: HTMLElement | null = event.target as HTMLElement;
    while (ele !== null && ele !== undefined && ele?.id !== 'root') {
        ele = ele.parentElement;
    }
    return Boolean(ele);
}

function isPanModifierPressed(event: KeyboardEvent): boolean {
    return isMac ? event.metaKey : event.ctrlKey;
}

const useInteractorMouseState = (chartInteractorRef: React.RefObject<ChartInteractorHandles>, scrollerRef: React.RefObject<HTMLDivElement>,
    session: Session, interactive?: boolean): InteractorMouseHandlers => {
    const clickPos = useRef<undefined | ExtendPos>(undefined);
    const lastPos = useRef<Pos | undefined>(undefined);
    const [interactorMouseState, setInteractorMouseState] = React.useState<InteractorMouseState>({ clickPos, lastPos });
    const onMouseMove = (e: React.MouseEvent): void => {
        if (!chartInteractorRef.current) { return; }
        const rect = e.currentTarget.getBoundingClientRect();
        const offsetX = e.nativeEvent.x - rect.left - LANE_INFO_WIDTH_PX.value;
        const offsetY = e.nativeEvent.y - rect.top;
        if (offsetX <= 0) {
            interactorMouseState.lastPos.current = interactorMouseState.clickPos.current
                ? { x: 0, y: offsetY, absoluteX: e.nativeEvent.x, absoluteY: e.nativeEvent.y }
                : undefined;
        } else {
            interactorMouseState.lastPos.current = { x: offsetX, y: offsetY, absoluteX: e.nativeEvent.x, absoluteY: e.nativeEvent.y };
        }
        chartInteractorRef.current.mouseMoveAction(interactorMouseState, e);
    };
    const onMouseDown = (e: React.MouseEvent): void => {
        const disabled = !isTargetElement(e) || !chartInteractorRef.current || !interactive ||
            session.phase !== 'download' || isMouseOnScrollbar(e, scrollerRef.current);
        if (disabled) {
            interactorMouseState.lastPos.current = undefined;
            return;
        }
        const needDragOneSide = chartInteractorRef.current.mouseDownAction(interactorMouseState, e);
        if (needDragOneSide === MouseDownActionResult.NEED_DRAG_ONE_SIDE) {
            // 当点击到已经有 selectedRange 的边界，需要触发拖拽
            onMouseMove(e);
        }
    };
    const onWheel = (e: React.WheelEvent<HTMLDivElement>): void => {
        if (!chartInteractorRef.current) { return; }
        chartInteractorRef.current.mouseWheelAction(interactorMouseState);
        setInteractorMouseState({ ...interactorMouseState, wheelEvent: { ctrlKey: e.ctrlKey, deltaY: e.deltaY } });
    };
    const onMouseUp = (e: MouseEvent): void => {
        if (!chartInteractorRef.current || !interactive) { return; }
        chartInteractorRef.current.mouseUpAction(interactorMouseState, e);
        if (session.sliceSelection.active) { session.sliceSelection.selecting = false; }
    };
    const onMouseLeave = (): void => {
        if (!chartInteractorRef.current) { return; }
        chartInteractorRef.current.mouseLeaveAction(interactorMouseState);
    };
    const onKeyDown = (e: React.KeyboardEvent<HTMLDivElement>): void => chartInteractorRef.current?.keyDownAction(e, interactorMouseState);
    return { onMouseMove, onMouseDown, onWheel, onMouseLeave, onMouseUp, onKeyDown, interactorMouseState };
};

interface InteractorMouseHandlers {
    onMouseUp: (e: MouseEvent) => void;
    onMouseMove: (e: React.MouseEvent) => void;
    onMouseLeave: () => void;
    onMouseDown: (e: React.MouseEvent) => void;
    onWheel: (e: React.WheelEvent<HTMLDivElement>) => void;
    onKeyDown: (e: React.KeyboardEvent<HTMLDivElement>) => void;
    interactorMouseState: InteractorMouseState;
}
