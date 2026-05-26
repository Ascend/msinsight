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

import React, { useEffect, useMemo, useRef, useState } from 'react';
import { runInAction } from 'mobx';
import {
    workerInitCanvas,
    workerResizeCanvas,
    workerTransform,
    workerHoverItem,
    workerClickItem,
    workerSetMemoryStateData,
    workerSelectItem as workerSelectStateItem,
} from '@/leaksWorker/stateWorker/worker';
import { workerSelectItem as workerSelectBlockItem } from '@/leaksWorker/blockWorker/worker';
import { Session } from '@/entity/session';
import { Input, Progress, ResizeTable, ResizeTableRef, SearchIcon } from '@insight/lib';
import { ColumnWidthOutlined, LeftOutlined, OneToOneOutlined, RightOutlined } from '@ant-design/icons';
import { Tooltip } from '@insight/lib/components';
import { type Theme, useTheme } from '@emotion/react';
import { formatBytes } from '@/utils/utils';
import { type EvenItem, getMemoryStateData, getSnapshotEvent } from '@/utils/RequestUtils';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled/macro';
import {
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
    Loading,
    StateHoverItem,
} from './tools';

export const MemoryStateDiagram = ({ session }: { session: Session }): JSX.Element => {
    return <div style={{ display: 'flex', height: 800 }}>
        <div style={{ width: 350 }}>
            <EventList session={session} />
        </div>
        <div style={{ flex: 1, paddingLeft: 30 }}>
            <StateDiagramCanvas session={session} />
        </div>
    </div>;
};

const EventItemRender = ({ record }: { record: EvenItem }): JSX.Element => {
    const commonStyle = { width: 100, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' };

    const computeSize = formatBytes(record.size);

    return <div style={{ display: 'flex', width: 320 }}>
        <div style={commonStyle} title={record.action}>{record.action}</div>
        <div style={{ width: 10 }}></div>
        <div>
            <div style={commonStyle} title={record.address}>{record.address}</div>
            <div style={commonStyle} title={`stream ${record.stream}`}>(stream {record.stream})</div>
        </div>
        <div style={{ width: 10 }}></div>
        <div>
            <div style={commonStyle} title={computeSize}>{computeSize}</div>
            <div style={commonStyle} title={`${record.size} B`}>({record.size} B)</div>
        </div>
    </div >;
};

let currentRequestId = 0;
const MIN_PAGE_SIZE = 1000;
const MAX_PAGE_SIZE = 64000; // 分页逐步增加，配合逻辑，最大值必须是 1000 * 2^n
const EventList = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [searchValue, setSearchValue] = useState<string>('');
    const [searchIndexList, setSearchIndexList] = useState<number[]>([]);
    const [dataSource, setDataSource] = useState<EvenItem[]>([]);
    const [dataTotal, setDataTotal] = useState<number>(0);
    const [currentShowRow, setCurrentShowRow] = useState<number>(-1);
    const [currentSelectRow, setCurrentSelectRow] = useState<number>(-1);
    const tableRef = useRef<ResizeTableRef>(null);
    const theme: Theme = useTheme();

    const columns = [{
        key: 'id',
        render: (_value: string, record: EvenItem, _index: number) => <EventItemRender record={record} />,
    }];

    const handleChange = (e: React.ChangeEvent<HTMLInputElement>): void => {
        const inputContent = e.target.value;
        if (/^[a-zA-Z0-9]*$/.test(inputContent)) {
            setSearchValue(inputContent);
        }
    };

    const handleSearch = (): void => {
        const result: number[] = [];
        if (searchValue.length < 1) {
            setSearchIndexList(result);
            setCurrentShowRow(-1);
            return;
        }
        for (let i = 0; i < dataSource.length; i++) {
            if (String(dataSource[i].address).includes(searchValue)) {
                result.push(i);
            }
        }
        setSearchIndexList(result);
        if (result.length > 0) {
            tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * result[0] });
            setCurrentShowRow(0);
        } else {
            setCurrentShowRow(-1);
        }
    };

    const [canUpRow, canDownRow] = useMemo(() => {
        return [
            currentShowRow > 0 && currentShowRow < searchIndexList.length,
            currentShowRow > -1 && currentShowRow < searchIndexList.length - 1,
        ];
    }, [currentShowRow, searchIndexList]);

    const changeScroll = (type: string = 'up'): void => {
        if (type === 'up' && !canUpRow) {
            return;
        }
        if (type === 'down' && !canDownRow) {
            return;
        }
        const offset = type === 'up' ? -1 : 1;
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * searchIndexList[currentShowRow + offset] });
        setCurrentShowRow(oVal => (oVal + offset));
    };

    const getAllEventListData = async (session: Session): Promise<void> => {
        runInAction(() => {
            session.loadingState = true;
        });
        currentRequestId = ++currentRequestId % 1000;
        const requestId = currentRequestId;

        let currentDataCount = 0;
        let currentPage = 1; // 初始页码设为 1
        let total = 0;
        let pageSize = MIN_PAGE_SIZE;
        let hasSwitchedToLargePage = false; // 标志位，表示是否已经切换到大页模式

        do {
            if (requestId !== currentRequestId) {
                return;
            }

            const res = await getSnapshotEvent({
                deviceId: session.deviceId,
                currentPage,
                pageSize,
            });

            if (requestId !== currentRequestId) {
                return;
            }

            const appendEventList = res.events.map((item, index) => ({
                ...item,
                index: currentDataCount + index,
            }));

            currentDataCount += appendEventList.length;
            total = res.total;
            setDataSource(prevData => prevData.concat(appendEventList as any));
            setDataTotal(res.total);
            // --- 核心逻辑 ---
            if (!hasSwitchedToLargePage) {
                if (currentPage === 1 && pageSize === MIN_PAGE_SIZE) { // 第一步：请求第一页 (1000:1)
                    currentPage = 2; // 下一次请求第二页
                } else if (currentPage === 2 && pageSize === MIN_PAGE_SIZE) { // 第二步：请求第二页 (1000:2)
                    pageSize = pageSize * 2; // 保持在第二页，但将页大小翻倍
                } else { // 第三步及以后：保持在第二页，持续翻倍页大小，直到达到上限或数据获取完毕
                    if (pageSize < MAX_PAGE_SIZE) {
                        pageSize = Math.min(pageSize * 2, MAX_PAGE_SIZE);
                    } else {
                        // 如果页大小已达到上限，就进入下一页
                        currentPage++;
                    }
                }
                // 如果当前页大小已达到最大值，说明后续应该切换到正常的翻页模式
                if (pageSize >= MAX_PAGE_SIZE) {
                    hasSwitchedToLargePage = true;
                }
            } else {
                // 如果已经切换到大页模式，则按正常逻辑翻页
                currentPage++;
            }
        } while (currentDataCount < total);
    };

    const setMemoryStateData = (): void => {
        const currentRow = dataSource[currentSelectRow];
        if (currentRow === undefined) {
            workerSetMemoryStateData({ data: [] });
            return;
        }
        getMemoryStateData({ eventId: currentRow.id, deviceId: session.deviceId }).then(data => {
            workerSetMemoryStateData({ data: data.segments });
            runInAction(() => {
                session.loadingState = false; // 实际只有第一次获取数据时才需要显示loading
            });
        });
    };

    useEffect(() => {
        if (session.deviceId === '') return;
        setDataSource([]);
        setCurrentSelectRow(0);
        setDataTotal(0);
        // table使用了不自动恢复滚动条模式，需要手动恢复到0
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 0 });
        workerSetMemoryStateData({ data: [] });
        getAllEventListData(session);
    }, [session.deviceId]);

    useEffect(() => {
        if (session.leaksWorkerInfo.clickItem === null) {
            return;
        }
        const index = dataSource.findIndex(item => item.id === session.leaksWorkerInfo.clickItem?.id);
        setCurrentSelectRow(index);
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * index });
    }, [session.leaksWorkerInfo.clickItem]);

    useEffect(() => {
        if (session.deviceId === '') return;
        setMemoryStateData();
    }, [currentSelectRow]);

    useEffect(() => {
        if (dataSource.length < 1001 && dataSource.length > 0) {
            // 事件列表第一次变化时，设置内存状态数据
            setMemoryStateData();
        }
    }, [dataSource]);

    return <div>
        <div style={{ display: 'flex', paddingBottom: 8 }}>
            <Input allowClear style={{ flex: 1 }} value={searchValue} onChange={handleChange} onPressEnter={handleSearch} placeholder={t('placeholderEventList')} />
            <div style={{ padding: '0 10px', display: 'flex', alignItems: 'center', cursor: 'pointer' }} onClick={handleSearch}>
                <SearchIcon />
            </div>
            <LeftOutlined style={{ color: theme.textColor, cursor: canUpRow ? 'pointer' : 'not-allowed' }} onClick={() => changeScroll()} />
            <div style={{ paddingLeft: 10 }} />
            <RightOutlined style={{ color: theme.textColor, cursor: canDownRow ? 'pointer' : 'not-allowed' }} onClick={() => changeScroll('down')} />
            <div style={{ paddingLeft: 10 }} />
        </div>
        <ProgressLine currentCount={dataSource.length} total={dataTotal} />
        <ResizeTable className="table-slice-list" ref={tableRef} virtual scroll={{ y: 760 }} dataSource={dataSource} rowKey={(row: EvenItem & { index: number }): string => `${row.id}_${row.index}`} columns={columns} showHeader={false}
            resetScroll={false} loading={dataSource.length < 1} allowCopy={false} rowClassName={(row: any): string => {
                if (currentSelectRow === row.index) {
                    return 'click-select';
                }
                return searchIndexList.includes(row.index) ? 'selected-row' : 'click-able';
            }}
            onRow={(row): React.HTMLAttributes<any> => ({
                onClick: (): void => {
                    setCurrentSelectRow(row.index);
                    const selectionVersion = session.selectionVersion + 1;
                    workerSelectBlockItem({ item: null, selectionVersion });
                    workerSelectStateItem({ item: null, selectionVersion });
                    runInAction(() => {
                        session.selectionVersion = selectionVersion;
                        session.leaksWorkerInfo.clickItem = null;
                        session.stateWorkerInfo.clickItem = null;
                        session.clickEventItem = row;
                    });
                },
            })}
        />
    </div>;
});

const ProgressContainer = styled.div`
    height: 30px;
    .ant-progress.ant-progress-line {
        width: 330px !important;
    }
`;
const ProgressLine = ({ currentCount, total }: { currentCount: number; total: number }): JSX.Element => {
    const formatPercent = (percent?: number): string => {
        if (percent === undefined) {
            return '';
        } else {
            return `${percent.toFixed(2)} %`;
        }
    };
    return <>
        {
            (total !== 0 && currentCount !== total)
                ? <ProgressContainer><Progress percent={currentCount / total * 100} format={formatPercent} /></ProgressContainer>
                : <></>
        }
    </>;
};

const DEFAULT_TRANSFORM: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
const STATE_X_ZOOM_STEP_MULTIPLIER = 3;
const getTransformScaleX = (transform: RenderOptions['transform']): number => transform.scaleX;
const getTransformScaleY = (transform: RenderOptions['transform']): number => transform.scaleY;

const StateDiagramCanvas = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const containerRef = useRef<HTMLDivElement>(null);
    const ref = useRef<HTMLCanvasElement>(null);
    const [hoverPoint, setHoverPoint] = useState({ x: -1, y: -1 });
    const [xZoomMode, setXZoomMode] = useState(true);
    const xZoomModeRef = useRef(true);
    const isDragging = useRef(false);
    const isClick = useRef(false);
    const dragStartPoint = useRef({ x: 0, y: 0 });

    const resetTransform = (): void => {
        runInAction(() => {
            session.stateWorkerInfo.renderOptions.transform = { ...DEFAULT_TRANSFORM };
        });
        workerTransform({ transform: { ...DEFAULT_TRANSFORM } });
        workerHoverItem({ clientX: -1, clientY: -1 });
    };

    const toggleXZoomMode = (): void => {
        setXZoomMode(mode => {
            const nextMode = !mode;
            xZoomModeRef.current = nextMode;
            return nextMode;
        });
    };

    const handleResize = (): void => {
        if (containerRef.current === null) {
            return;
        }
        const containerRect = containerRef.current.getBoundingClientRect();
        const width = containerRect.width;
        const height = containerRect.height;
        runInAction(() => {
            session.stateWorkerInfo.renderOptions.viewport = { width, height };
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
        const mouseY = ev.clientY - rect.top;

        // 获取当前变换参数
        const currentTransform = session.stateWorkerInfo.renderOptions.transform;
        const currentScaleX = getTransformScaleX(currentTransform);
        const currentScaleY = getTransformScaleY(currentTransform);
        const onlyScaleX = ev.shiftKey || (!ev.ctrlKey && xZoomModeRef.current);

        // 计算缩放前鼠标在实际内容中的相对位置（相对于画布原点）
        const originalContentMouseX = (mouseX - currentTransform.x) / currentScaleX;
        const originalContentMouseY = (mouseY - currentTransform.y) / currentScaleY;

        // 计算新的缩放值
        const deltaScale = (ev.deltaY > 0 ? -0.1 : 0.1) * (onlyScaleX ? STATE_X_ZOOM_STEP_MULTIPLIER : 1);
        const newScaleX = Math.max(0.1, currentScaleX + deltaScale);
        const newScaleY = onlyScaleX ? currentScaleY : Math.max(0.1, currentScaleY + deltaScale);

        const maxRangeX = rect.width;
        const minRangeX = -rect.width * newScaleX;
        const maxRangeY = rect.height;
        const minRangeY = -rect.height * newScaleY;
        // 计算缩放后的新偏移，使鼠标下的内容位置不变
        // 原始偏移距离 + (内容相对位置 * (新缩放 - 旧缩放))
        const newX = Math.min(Math.max(mouseX - originalContentMouseX * newScaleX, minRangeX), maxRangeX);
        const newY = onlyScaleX ? currentTransform.y : Math.min(Math.max(mouseY - originalContentMouseY * newScaleY, minRangeY), maxRangeY);

        // 更新变换参数
        const transform = { x: newX, y: newY, scaleX: newScaleX, scaleY: newScaleY };
        runInAction(() => {
            session.stateWorkerInfo.renderOptions.transform = transform;
        });

        workerTransform({ transform });
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
        isDragging.current = false;
        isClick.current = false;
        setHoverPoint({ x: -1, y: -1 });
        workerHoverItem({ clientX: -1, clientY: -1 });
        runInAction(() => {
            session.stateWorkerInfo.hoverItem = null;
        });
    };

    const handleMouseMove = (ev: MouseEvent): void => {
        if (ref.current === null) {
            return;
        }
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
            workerHoverItem({ clientX: currentX, clientY: currentY });
            setHoverPoint({ x: currentX, y: currentY });
            return;
        }
        setHoverPoint({ x: -1, y: -1 });

        const currentTransform = session.stateWorkerInfo.renderOptions.transform;
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
            y: Math.min(Math.max(currentTransform.y + deltaY, minRangeY), maxRangeY),
        };
        runInAction(() => {
            session.stateWorkerInfo.renderOptions.transform = transform;
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
            const selectionVersion = session.selectionVersion + 1;
            workerSelectBlockItem({ item: null, selectionVersion });
            runInAction(() => {
                session.selectionVersion = selectionVersion;
                session.leaksWorkerInfo.clickItem = null;
                session.clickEventItem = null;
            });
            workerClickItem({ clientX: ev.clientX - rect.left, clientY: ev.clientY - rect.top, selectionVersion });
        }
    };

    const handleKeyDown = (ev: KeyboardEvent): void => {
        if (ref.current !== null && (ev.ctrlKey || ev.shiftKey) && ['+', '=', '-', '_'].includes(ev.key)) {
            ev.preventDefault();
            const rect = ref.current.getBoundingClientRect();
            const direction = ev.key === '-' || ev.key === '_' ? -1 : 1;
            const onlyScaleX = ev.shiftKey && !ev.ctrlKey;
            const mouseX = rect.width / 2;
            const mouseY = rect.height / 2;
            const currentTransform = session.stateWorkerInfo.renderOptions.transform;
            const currentScaleX = getTransformScaleX(currentTransform);
            const currentScaleY = getTransformScaleY(currentTransform);
            const originalContentMouseX = (mouseX - currentTransform.x) / currentScaleX;
            const originalContentMouseY = (mouseY - currentTransform.y) / currentScaleY;
            const deltaScale = 0.1 * direction;
            const newScaleX = Math.max(0.1, currentScaleX + deltaScale);
            const newScaleY = onlyScaleX ? currentScaleY : Math.max(0.1, currentScaleY + deltaScale);
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
                session.stateWorkerInfo.renderOptions.transform = transform;
            });
            workerTransform({ transform });
            return;
        }
        if (ev.key.toLowerCase() === 'r') {
            resetTransform();
            return;
        }
        if (ev.key.toLowerCase() === 'h') {
            toggleXZoomMode();
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
        if (ref.current === null || containerRef.current === null) {
            return;
        }
        const canvas = ref.current;
        canvas.tabIndex = 0;
        try {
            const containerRect = containerRef.current.getBoundingClientRect();
            const width = containerRect.width;
            const height = containerRect.height;

            runInAction(() => {
                session.stateWorkerInfo.renderOptions.viewport = { width, height };
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

    return <div style={{ position: 'relative', display: 'flex', flexDirection: 'column', width: '100%', height: '100%', paddingTop: 36 }}>
        <GraphToolbarTooltipStyle />
        <GraphToolbar>
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
        <div data-testid="stateDiagram" ref={containerRef} style={{ position: 'relative', flex: 1, overflow: 'hidden' }}>
            <canvas
                ref={ref}
                style={{ position: 'absolute', top: 0, imageRendering: 'pixelated', touchAction: 'none', outline: 'none' }}
            />
            <StateHoverItem session={session} point={hoverPoint} />
            <Loading style={{ position: 'absolute', top: 0, left: 0, width: '100%', height: '100%' }} loading={session.loadingState} />
        </div>
    </div>;
});
