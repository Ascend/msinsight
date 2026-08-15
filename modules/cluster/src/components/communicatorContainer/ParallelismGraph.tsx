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

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { queryParallelismArrangementCancelable } from '../../utils/RequestUtils';
import { ParallelismArrangementResult } from '../../utils/interface';
import { Session } from '../../entity/session';
import { observer } from 'mobx-react';
import { CanvasDrawer, Frame, Line, Rectangle } from './shape';
import { runInAction } from 'mobx';
import { getDyeingColor, groupFrames } from './ContainerUtils';
import eventBus, { useEventBus } from '../../utils/eventBus';
import styled from '@emotion/styled';
import { useParallelSwitchConditions } from './Context';
import { useTheme } from '@emotion/react';
import { throttle } from 'lodash';
import { ContextMenu, DynamicTooltip, Responsive } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { message, Spin } from 'antd';
import { transformCardIdInfo } from '@insight/lib/utils';
import parallelismStore, { type GenerateConditions } from '../../store/parallelism';
import { useContextMenuItems } from './useContextMenuItems';

const SCROLL_BAR_WIDTH = 10;

const CanvasContainer = styled.div`
    max-height: 800px;
    overflow: auto;
    background-color: ${(props): string => props.theme.rankContainerBackgroudColor};
`;

const Canvas = styled.canvas`
    position: absolute;
    right: ${SCROLL_BAR_WIDTH}px;
    bottom: ${SCROLL_BAR_WIDTH}px;
    width: calc(100% - ${SCROLL_BAR_WIDTH}px);
    height: calc(100% - ${SCROLL_BAR_WIDTH}px);
    pointer-events: none;
`;

export const Loading = styled.div`
    z-index: 1;
    display: flex;
    position: absolute;
    width: 100%;
    height: 100%;
    background-color: ${(props): string => props.theme.maskColor};
    color: ${(props): string => props.theme.textColorPrimary};

    > div {
        margin: auto;
    }
`;

interface Position {
    left: number;
    top: number;
    width: number;
    height: number;
}
export const useLocateAnim = (containerRef: React.RefObject<HTMLElement>): (pos: Position) => void => {
    return useCallback(({ top, left, width, height }: Position) => {
        const animBox = document.createElement('div');
        animBox.className = 'zoom-anim';

        Object.assign(animBox.style, {
            position: 'absolute',
            top: `${top}px`,
            left: `${left}px`,
            width: `${width}px`,
            height: `${height}px`,
            border: '2px solid rgb(100, 220, 150)',
            pointerEvents: 'none',
            zIndex: '999999',
            animation: 'blink 1.2s ease-out forwards',
        });

        containerRef.current?.appendChild(animBox);

        setTimeout((): void => {
            animBox.remove();
        }, 1500);
    }, [containerRef]);
};

const resetPerformanceConditions = (): void => {
    eventBus.emit('resetPerformanceConditions');
};

interface UseFetchDataReturns {
    loading: boolean;
    isUpdated: boolean;
    data?: ParallelismArrangementResult;
    error?: Error;
}

const useFetchData = (params: GenerateConditions | null, session: Session): UseFetchDataReturns => {
    const [data, setData] = useState<ParallelismArrangementResult>();
    const [loading, setLoading] = useState<boolean>(false);
    const [error, setError] = useState<Error>();
    const [isUpdated, setIsUpdated] = useState<boolean>(false);

    const fetchData = async (): Promise<void> => {
        if (params === null) {
            return;
        }

        const { invoke } = queryParallelismArrangementCancelable;
        try {
            setLoading(true);
            const res = await invoke(params);
            setData(res);
            setIsUpdated((oldV) => !oldV);
        } catch (err) {
            setError(err as Error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        if (!session?.isClusterPathInList) {
            return;
        }
        const isDefaultParams = params?.dpSize === 1 && params?.tpSize === 1 && params?.ppSize === 1 && params.cpSize === 1 && params?.epSize === 1;
        if (params !== null && !isDefaultParams) {
            fetchData();
        }
        const unitcount = session?.unitcount;
        if (unitcount && unitcount <= 64 && isDefaultParams) {
            fetchData();
        }
    }, [JSON.stringify(params), session?.isClusterPathInList]);

    return { data, loading, error, isUpdated };
};

interface TooltipsDataProps {
    session: Session;
    hoveredRectIndex: number | null;
    data?: ParallelismArrangementResult;
    dimension?: GenerateConditions['dimension'];
}
const useTooltipsData = ({ hoveredRectIndex, data, session, dimension }: TooltipsDataProps): Record<string, string> | null => {
    const { t } = useTranslation('summary');

    return useMemo(
        () => {
            if (hoveredRectIndex === null || data === undefined) {
                return null;
            }

            const { name, index, formattedRanks } = data.arrangements[hoveredRectIndex];
            const currentData = session.performanceDataMap?.get(hoveredRectIndex);
            if (currentData === undefined) {
                return {
                    [t('Index')]: index,
                    [t('Name')]: name,
                };
            }

            const updatedData: Record<string, any> = {};

            for (const indicatorItem of session.indicatorMap.values()) {
                const { key, name: indicatorName, unit } = indicatorItem;
                const value = currentData[key];
                if (value !== undefined) {
                    updatedData[t(indicatorName)] =
                        session.isCompare
                            ? <span>{value}<span className={currentData.diff[key] >= 0 ? 'positive-number' : 'negative-number'}>({currentData.diff[key]})</span> {unit}</span>
                            : `${value} ${unit}`;
                }
            }
            for (const key of Object.keys(currentData.commCompare)) {
                const diffValue = currentData.commDiff[key];
                const value = currentData.commCompare[key];
                const curName = dimension === 'ep-dp-pp-cp-tp' ? `${key.toUpperCase()}-${t('Communication')}` : `${key.toUpperCase()}-${t('Avg Communication')}`;
                updatedData[curName] = session.isCompare
                    ? <span>{value}<span className={diffValue >= 0 ? 'positive-number' : 'negative-number'}>({diffValue})</span> μs</span>
                    : `${value} μs`;
            }

            return {
                [t('Index')]: index,
                [t('Name')]: name,
                [t('Included Ranks')]: formattedRanks,
                ...updatedData,
            };
        },
        [hoveredRectIndex],
    );
};

const getFirstAndLastRect = (drawer: CanvasDrawer): [Rectangle | undefined, Rectangle | undefined] => {
    const { rectToExpand, rectToCollapsed } = parallelismStore;

    if (rectToExpand) {
        const first = drawer.rectangles.find(r => r.name.startsWith(`${rectToExpand.name}-`));
        const last = drawer.rectangles.slice().reverse().find(r => r.name.startsWith(`${rectToExpand.name}-`));
        return [first, last];
    }

    if (rectToCollapsed) {
        let first = drawer.rectangles.find(r => r.name === rectToCollapsed.name);
        if (!first) {
            const name = rectToCollapsed.name.substring(0, rectToCollapsed.name.lastIndexOf('-'));
            first = drawer.rectangles.find(r => r.name.startsWith(name));
        }
        return [first, undefined];
    }

    return [undefined, undefined];
};

interface ParallelismGraphProps {
    session: Session;
    targetRankIndex: number | null;
    targetTrigger: boolean;
}
export const ParallelismGraph = observer(({ session, targetRankIndex, targetTrigger }: ParallelismGraphProps): JSX.Element => {
    const { generateConditions } = parallelismStore;
    const canvasContainerRef = useRef<HTMLDivElement>(null);
    const mainCanvasRef = useRef<HTMLCanvasElement>(null);
    const hoverCanvasRef = useRef<HTMLCanvasElement>(null);
    const lastMousePositionRef = useRef<{ x: number; y: number }>({ x: 0, y: 0 });
    const [canvasDrawer, setCanvasDrawer] = useState<CanvasDrawer | null>(null);
    const [lastRect, setLastRect] = useState<Rectangle>();
    const [activeRectIndex, setActiveRectIndex] = useState<number | null>(null);
    const [hoveredRectIndex, setHoveredRectIndex] = useState<number | null>(null);
    const [responsiveSize, setResponsiveSize] = useState({ width: 0, height: 0 });
    const { parallelTypeList, dyeingMode, setDyeingMode, startVal, endVal } = useParallelSwitchConditions();
    const theme = useTheme();
    const { data, loading, isUpdated } = useFetchData(generateConditions, session);
    const { tpSize = 1, dpSize = 1, cpSize = 1, epSize = 1, ppSize = 1, dimension } = generateConditions ?? {};
    const locateTargetAnim = useLocateAnim(canvasContainerRef);
    const [contextMenuRect, setContextMenuRect] = useState<Rectangle | null>(null);
    const [contextMenuLine, setContextMenuLine] = useState<Line | null>(null);
    const [autoActiveNum, setAutoActiveNum] = useState<number>(-1);
    const tooltipsData = useTooltipsData({
        hoveredRectIndex,
        data,
        session,
        dimension,
    });
    const contextMenuItems = useContextMenuItems({
        session,
        activeRect: contextMenuRect,
        activeLine: contextMenuLine,
        tooltipsData,
    });

    const canvasSize = useMemo(() => {
        let width = 200;
        let height = 200;

        if (lastRect !== undefined) {
            const { x, y, width: rectWidth, height: rectHeight, textHeight } = lastRect;
            const actualWidth = x + rectWidth + CanvasDrawer.PADDING;
            const actualHeight = y + rectHeight + CanvasDrawer.PADDING + textHeight;

            width = Math.max(actualWidth, width);
            height = Math.max(actualHeight, height);
        }
        return { width, height };
    }, [lastRect]);

    const render = (): void => {
        const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef.current ?? {};
        canvasDrawer?.render(scrollLeft, scrollTop);
    };

    const addRectangles = (drawer: CanvasDrawer): void => {
        data?.arrangements.forEach(arrangement => {
            drawer?.addRectangle(
                new Rectangle({
                    index: arrangement.index,
                    name: arrangement.name,
                    rowAndCol: arrangement.position,
                    fillColor: dyeingMode === 'None'
                        ? undefined
                        : getDyeingColor({
                            session,
                            index: arrangement.index,
                            dyeingMode,
                            range: [startVal, endVal],
                        }),
                    backgroundColor: theme.bgColorLighter,
                    color: theme.textColorPrimary,
                    attribute: {
                        ...arrangement.attribute,
                        tpSize,
                        dpSize,
                        cpSize,
                        epSize,
                        ppSize,
                    },
                }));
        });
    };

    const addFrames = (drawer: CanvasDrawer): void => {
        if (drawer !== null && data !== undefined) {
            const framesGroup = groupFrames(data.arrangements, parallelTypeList, data.connections);

            drawer.clearFrames();
            framesGroup.forEach(frame => {
                const { type, list } = frame;
                const frameInst = new Frame(type, list, {
                    tpSize,
                    dpSize,
                    cpSize,
                    epSize,
                    ppSize,
                });

                drawer.addFrame(frameInst);
            });
        }
    };

    const addLines = (drawer: CanvasDrawer): void => {
        if (drawer !== null && data !== undefined && activeRectIndex !== null) {
            const linesGroup = data?.connections?.filter(connection => {
                const isPpLineInPpDimension = (dimension === 'ep-dp-pp-cp' || dimension === 'ep-dp-pp') &&
                    connection.type === 'pp';
                return connection.list.includes(activeRectIndex) && !isPpLineInPpDimension;
            }).map(item => {
                return {
                    type: item.type,
                    list: item.list.map(it => ({
                        index: it,
                        position: data.arrangements[it].position,
                        attribute: data.arrangements[it].attribute,
                    })),
                };
            }) ?? [];

            linesGroup?.forEach(line => {
                const { type, list } = line;
                const parallelismSize = {
                    tpSize,
                    dpSize,
                    cpSize,
                    epSize,
                    ppSize,
                };
                const lineInst = new Line(type, list, parallelismSize);
                drawer.addLine(lineInst);
            });
        }
    };

    const handleScrollToTarget = useCallback(() => {
        if (targetRankIndex === null) {
            return;
        }

        const targetRank = canvasDrawer?.rectangles[targetRankIndex];

        if (!targetRank) {
            message.warning('Target not found');
            return;
        }

        const { originalX, originalY, width: rectWidth, height: rectHeight } = targetRank;
        const { width: containerWidth, height: containerHeight } = responsiveSize;
        const xCoord = Math.floor(originalX - (containerWidth / 2));
        const yCoord = Math.floor(originalY - (containerHeight / 2));

        canvasContainerRef.current?.scrollTo(xCoord, yCoord);

        const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef.current ?? {};
        const top = originalY - scrollTop;
        const left = originalX - scrollLeft;

        locateTargetAnim({
            top,
            left,
            width: rectWidth,
            height: rectHeight,
        });
        if (autoActiveNum > -1) {
            setActiveRectIndex(autoActiveNum);
            setAutoActiveNum(-1);
        }
    }, [canvasDrawer, targetRankIndex]);

    useEffect(() => {
        if (mainCanvasRef.current && data !== undefined) {
            const drawer = new CanvasDrawer(mainCanvasRef, hoverCanvasRef);
            setCanvasDrawer(drawer);
            addRectangles(drawer);
            addFrames(drawer);
            setLastRect(drawer.rectangleList[drawer.rectangleList.length - 1]);
            setTimeout(() => {
                drawer.render(canvasContainerRef.current?.scrollLeft ?? 0, canvasContainerRef.current?.scrollTop ?? 0);
                scrollToRect(drawer);
            });
            const rankDbPathMap: Map<string, string> = new Map();
            const getRealRankId = (cardId: string): string => {
                const cardInfo = transformCardIdInfo(cardId);
                return cardInfo.rankName !== '' ? cardInfo.rankName : cardInfo.deviceId;
            };
            data.rankDbPathList?.forEach((item) => rankDbPathMap.set(getRealRankId(item.rankId), item.dbPath));
            // 更新所有通信域数据、指标数据
            runInAction(() => {
                const connections = data.connections.map(item => item.list.toString());
                const ppConnections = data.connections.filter(item => item.type === 'pp').map(item => item.list.toString());
                const frames = groupFrames(data.arrangements, ['dp', 'ep', 'cp'])
                    .map(frameGroup => frameGroup.list.map(item => item.index).toString());

                session.rankDbPathMap = rankDbPathMap;
                session.communicationDomains = [...new Set([...connections, ...frames])];
                session.ppCommunicationDomains = ppConnections;
                session.indicatorList = data?.indicators.map(indicator => {
                    const unit = indicator.yAxisType === 'time' ? 'μs' : '%';
                    return {
                        ...indicator,
                        unit,
                    };
                }) ?? [];
                session.arrangementRankCount = data?.size || 0;
                session.setRankDyeingData();
            });

            setActiveRectIndex(null);
            resetPerformanceConditions();
        }
    }, [isUpdated]);

    useEffect(() => {
        const dyeingMode = dyeingModeMapping[parallelismStore.activeDimension] || 'None';
        if (dyeingMode in session.rankDyeingData) {
            setDyeingMode(dyeingMode);
        } else {
            setDyeingMode('None');
        }
    }, [parallelismStore.activeDimension, session.rankDyeingData]);

    const dyeingModeMapping = {
        'ep-dp': 'None',
        'ep-dp-pp': 'dp',
        'ep-dp-pp-cp': 'cp',
        'ep-dp-pp-tp': 'tp',
        'ep-dp-pp-cp-tp': 'tp',
    };

    const scrollToRect = (drawer: CanvasDrawer): void => {
        const [firstExpandedRect, lastExpandedRect] = getFirstAndLastRect(drawer);

        if (!firstExpandedRect) {
            return;
        }

        const { originalX, originalY, width: rectWidth, height: rectHeight } = firstExpandedRect;
        const { width: containerWidth, height: containerHeight } = responsiveSize;

        const xCoord = Math.floor(originalX - (containerWidth / 4));
        const yCoord = Math.floor(originalY - (containerHeight / 4));
        // FIX: 修复当并行策略图图形数量大时，渲染耗时长，影响 scrollTo 执行时机太早导致失效的问题
        // 这里将 scrollTo 的行为放在宏任务（setTimeout）中，后置执行时机解决
        setTimeout(() => {
            canvasContainerRef.current?.scrollTo(xCoord, yCoord);

            const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef.current ?? {};
            const top = originalY - scrollTop;
            const left = originalX - scrollLeft;

            let width = rectWidth;
            let height = rectHeight;

            if (lastExpandedRect) {
                width += lastExpandedRect.originalX - originalX;
                height += lastExpandedRect.originalY - originalY;
            }

            locateTargetAnim({
                top,
                left,
                width,
                height,
            });
        });

        runInAction(() => {
            parallelismStore.rectToExpand = null;
            parallelismStore.rectToCollapsed = null;
        });
    };

    useEffect(() => {
        if (data !== undefined && canvasDrawer !== null) {
            canvasDrawer.clearRectangles();
            addRectangles(canvasDrawer);
            render();
        }
    }, [theme.mode, dyeingMode, startVal, endVal]);

    useEffect(() => {
        if (canvasDrawer !== null) {
            canvasDrawer.clearLines();
            addLines(canvasDrawer);
            render();
        }
    }, [activeRectIndex]);

    useEffect(() => {
        if (canvasDrawer !== null && data?.arrangements !== undefined) {
            addFrames(canvasDrawer);
            render();
        }
    }, [JSON.stringify(parallelTypeList)]);

    useEffect(() => {
        render();
    }, [responsiveSize.width, responsiveSize.height, canvasSize.width, canvasSize.height]);

    useEffect(() => {
        handleScrollToTarget();
    }, [targetTrigger]);

    useEventBus('selectSlowRanksTopNum', (num) => {
        setAutoActiveNum(num as number);
    });

    const onClickCanvas: React.MouseEventHandler<HTMLDivElement> = (event): void => {
        const { offsetX, offsetY } = event.nativeEvent;
        const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef.current ?? {};
        const x = offsetX - scrollLeft;
        const y = offsetY - scrollTop;

        if (canvasDrawer === null) {
            return;
        }

        // 判断矩形
        for (const rect of canvasDrawer.visibleRectangleList) {
            if (rect.isInside(x, y)) {
                setActiveRectIndex(rect.index);
                return;
            }
        }

        // 判断线
        for (const line of canvasDrawer.lineList) {
            if (line.isInside(x, y)) {
                eventBus.emit('activeCommunicator', {
                    name: line.type === 'pp' ? 'pipeline' : line.type,
                    value: line.rectList.map(rect => rect.index).join(','),
                });
                return;
            }
        }

        // 判断框
        for (const frame of canvasDrawer.frameList) {
            if (frame.isInside(x, y)) {
                eventBus.emit('activeCommunicator', {
                    name: frame.type,
                    value: frame.rectList.map(rect => rect.index).join(','),
                });
                return;
            }
        }

        setActiveRectIndex(null);
    };

    // 鼠标移入卡时，选中当前卡
    const setRectActive = useCallback(throttle((x, y): void => {
        const activeRect = canvasDrawer?.visibleRectangleList.find(rect => rect.isInside(x, y));

        setHoveredRectIndex(activeRect === undefined ? null : activeRect.index);
    }, 100), [canvasDrawer]);

    const onMouseMove: React.MouseEventHandler<HTMLDivElement> = (event) => {
        const { offsetX, offsetY } = event.nativeEvent;
        const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef?.current ?? {};
        const x = offsetX - scrollLeft;
        const y = offsetY - scrollTop;

        setRectActive(x, y);
        lastMousePositionRef.current = { x, y };
        canvasDrawer?.renderHoverCanvas(x, y);
    };

    const onMouseOut: React.MouseEventHandler<HTMLDivElement> = (): void => {
        setHoveredRectIndex(null);
    };

    const onScroll = (): void => {
        requestAnimationFrame(render);

        // 获取当前鼠标位置
        const { x, y } = lastMousePositionRef.current;
        setRectActive(x, y);
    };

    const handleResize = (size: {width: number;height: number}): void => {
        setResponsiveSize(size);
    };

    const handleContextMenuShow = ({ x: clientX, y: clientY }: { x: number; y: number }): void => {
        if (!canvasDrawer) return;
        const { scrollLeft = 0, scrollTop = 0 } = canvasContainerRef.current ?? {};
        const x = clientX - scrollLeft;
        const y = clientY - scrollTop;

        const rect = canvasDrawer.visibleRectangleList.find(r => r.isInside(x, y));
        setContextMenuRect(rect ?? null);

        const line = canvasDrawer.lineList.find(l => l.isInside(x, y));
        setContextMenuLine(line ?? null);
    };

    return <div className="parallelism-graph" style={{ position: 'relative' }}>
        {loading && <Loading data-testid="parallelism-graph-loading"><Spin /></Loading>}
        <Responsive onChange={handleResize}>
            {
                ({ width, height }): React.ReactNode => {
                    return <CanvasContainer ref={canvasContainerRef} onScroll={onScroll}>
                        <Canvas
                            ref={mainCanvasRef}
                            width={(width - SCROLL_BAR_WIDTH) * devicePixelRatio}
                            height={(height - SCROLL_BAR_WIDTH) * devicePixelRatio}
                        ></Canvas>
                        <Canvas
                            ref={hoverCanvasRef}
                            width={(width - SCROLL_BAR_WIDTH) * devicePixelRatio}
                            height={(height - SCROLL_BAR_WIDTH) * devicePixelRatio}
                        ></Canvas>
                        <ContextMenu
                            menuItems={contextMenuItems}
                            onShow={handleContextMenuShow}
                        >
                            <div
                                data-testid="parallelism-graph-placeholder"
                                style={{
                                    width: canvasSize.width,
                                    height: canvasSize.height,
                                }}
                                onClick={onClickCanvas}
                                onMouseMove={onMouseMove}
                                onMouseOut={onMouseOut}
                            ></div>
                        </ContextMenu>
                    </CanvasContainer>;
                }
            }
        </Responsive>
        <DynamicTooltip
            mouseX={lastMousePositionRef.current.x}
            mouseY={lastMousePositionRef.current.y}
            content={tooltipsData}
        />
    </div>;
});
