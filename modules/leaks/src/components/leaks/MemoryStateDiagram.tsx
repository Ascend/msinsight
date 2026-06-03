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
import { Progress, ResizeTable, ResizeTableRef } from '@insight/lib';
import { ArrowDownOutlined, ArrowUpOutlined, CloseCircleFilled, ColumnWidthOutlined, OneToOneOutlined, SearchOutlined } from '@ant-design/icons';
import { Tooltip } from '@insight/lib/components';
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
import { message } from 'antd';

export const MemoryStateDiagram = ({ session }: { session: Session }): JSX.Element => {
    return <div data-testid="stateDiagramSection" style={{ display: 'flex', height: 800 }}>
        <div style={{ width: 350 }}>
            <EventList session={session} />
        </div>
        <div style={{ flex: 1, padding: '0 30px' }}>
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
type EventSearchField = 'address' | 'eventType';
const EventList = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [searchField, setSearchField] = useState<EventSearchField | null>(null);
    const [searchFieldOrder, setSearchFieldOrder] = useState<EventSearchField[]>([]);
    const [searchValues, setSearchValues] = useState<Record<EventSearchField, string>>({ address: '', eventType: '' });
    const [draftSearchValues, setDraftSearchValues] = useState<Record<EventSearchField, string>>({ address: '', eventType: '' });
    const [isFilterMenuOpen, setIsFilterMenuOpen] = useState<boolean>(false);
    const [isEventTypeMenuOpen, setIsEventTypeMenuOpen] = useState<boolean>(false);
    const [searchIndexList, setSearchIndexList] = useState<number[]>([]);
    const [dataSource, setDataSource] = useState<EvenItem[]>([]);
    const [dataTotal, setDataTotal] = useState<number>(0);
    const [currentShowRow, setCurrentShowRow] = useState<number>(-1);
    const [currentSelectRow, setCurrentSelectRow] = useState<number>(-1);
    const tableRef = useRef<ResizeTableRef>(null);
    const findPanelRef = useRef<HTMLDivElement>(null);
    const searchInputRefs = useRef<Record<EventSearchField, HTMLInputElement | null>>({ address: null, eventType: null });
    const eventTypeButtonRef = useRef<HTMLButtonElement>(null);
    const addFilterAnchorRef = useRef<HTMLInputElement>(null);

    const columns = [{
        key: 'id',
        render: (_value: string, record: EvenItem, _index: number) => <EventItemRender record={record} />,
    }];

    const normalizeSearchValue = (value: string): string => value.replace(/\s+/g, '');

    const handleSearchChange = (field: EventSearchField, e: React.ChangeEvent<HTMLInputElement>): void => {
        const value = normalizeSearchValue(e.target.value);
        setDraftSearchValues(prevValues => ({ ...prevValues, [field]: value }));
    };

    const applyAddressPaste = (value: string): void => {
        const nextValue = normalizeSearchValue(value);
        if (nextValue.length < 1) {
            return;
        }
        setDraftSearchValues(prevValues => ({ ...prevValues, address: nextValue }));
        setSearchValues(prevValues => ({ ...prevValues, address: nextValue }));
        setSearchFieldOrder(prevOrder => prevOrder.includes('address') ? prevOrder : [...prevOrder, 'address']);
        setSearchField('address');
        setIsFilterMenuOpen(false);
        setIsEventTypeMenuOpen(false);
        requestAnimationFrame(() => searchInputRefs.current.address?.focus());
    };

    const handleDefaultSearchPaste = (ev: React.ClipboardEvent<HTMLInputElement>): void => {
        ev.preventDefault();
        applyAddressPaste(ev.clipboardData.getData('text'));
    };

    const applySearchField = (field: EventSearchField | null = searchField): void => {
        if (field === null) {
            return;
        }
        const nextValue = draftSearchValues[field];
        setSearchValues(prevValues => ({ ...prevValues, [field]: nextValue }));
        if (nextValue.trim().length < 1) {
            setSearchFieldOrder(prevOrder => prevOrder.filter(item => item !== field));
            if (searchField === field) {
                setSearchField(null);
            }
        }
    };

    const openEventTypeMenu = (): void => {
        setDraftSearchValues(prevValues => ({ ...prevValues, eventType: searchValues.eventType }));
        setSearchField('eventType');
        setIsFilterMenuOpen(false);
        setIsEventTypeMenuOpen(true);
        requestAnimationFrame(() => eventTypeButtonRef.current?.focus());
    };

    const editSearchField = (field: EventSearchField, shouldAppend: boolean = false): void => {
        if (shouldAppend) {
            setSearchFieldOrder(prevOrder => prevOrder.includes(field) ? prevOrder : [...prevOrder, field]);
        }
        if (field === 'eventType') {
            openEventTypeMenu();
            return;
        }
        setDraftSearchValues(prevValues => ({ ...prevValues, [field]: searchValues[field] }));
        setSearchField(field);
        setIsFilterMenuOpen(false);
        requestAnimationFrame(() => {
            searchInputRefs.current[field]?.focus();
        });
    };

    const handleSearchFieldClick = (): void => {
        const hasAvailableSearchField = Object.entries(searchValues).some(([field, value]) => field !== searchField && value.trim().length < 1 && draftSearchValues[field as EventSearchField].trim().length < 1);
        const currentFieldApplied = searchField !== null &&
            searchValues[searchField].trim().length > 0 &&
            searchValues[searchField] === draftSearchValues[searchField];
        if ((searchField === null || currentFieldApplied) && hasAvailableSearchField) {
            setIsFilterMenuOpen(true);
            requestAnimationFrame(() => addFilterAnchorRef.current?.focus());
            return;
        }
        setIsFilterMenuOpen(false);
        if (searchField !== null) {
            if (searchField === 'eventType') {
                openEventTypeMenu();
                return;
            }
            searchInputRefs.current[searchField]?.focus();
        }
    };

    const getMatchedIndexes = (): number[] => {
        const result: number[] = [];
        const addressKeyword = normalizeSearchValue(searchValues.address).toLowerCase();
        const eventTypeKeyword = normalizeSearchValue(searchValues.eventType).toLowerCase();
        if (addressKeyword.length < 1 && eventTypeKeyword.length < 1) {
            return result;
        }
        for (let i = 0; i < dataSource.length; i++) {
            const item = dataSource[i];
            const addressText = String(item.address ?? '').toLowerCase();
            const eventTypeText = String(item.action ?? '').toLowerCase();
            const addressMatched = addressKeyword.length < 1 || addressText.includes(addressKeyword);
            const eventTypeMatched = eventTypeKeyword.length < 1 || eventTypeText === eventTypeKeyword;
            if (addressMatched && eventTypeMatched) {
                result.push(i);
            }
        }
        return result;
    };

    const [canUpRow, canDownRow] = useMemo(() => {
        return [
            currentShowRow > 0 && currentShowRow < searchIndexList.length,
            currentShowRow > -1 && currentShowRow < searchIndexList.length - 1,
        ];
    }, [currentShowRow, searchIndexList]);

    const scrollToSearchResult = (matchIndex: number, shouldSelect: boolean = false): void => {
        const rowIndex = searchIndexList[matchIndex];
        if (rowIndex === undefined) {
            return;
        }
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * rowIndex });
        setCurrentShowRow(matchIndex);
        if (shouldSelect) {
            setCurrentSelectRow(rowIndex);
            selectEventRow(dataSource[rowIndex]);
        }
    };

    const changeScroll = (type: string = 'up'): void => {
        if (type === 'up' && !canUpRow) {
            return;
        }
        if (type === 'down' && !canDownRow) {
            return;
        }
        scrollToSearchResult(currentShowRow + (type === 'up' ? -1 : 1), true);
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

    const selectEventRow = (row: EvenItem & { index?: number }): void => {
        const selectionVersion = session.selectionVersion + 1;
        workerSelectBlockItem({ item: null, selectionVersion });
        workerSelectStateItem({ item: null, selectionVersion });
        runInAction(() => {
            session.selectionVersion = selectionVersion;
            session.leaksWorkerInfo.clickItem = null;
            session.stateWorkerInfo.clickItem = null;
            session.clickEventItem = row;
        });
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
        setSearchIndexList([]);
        setCurrentShowRow(-1);
        setCurrentSelectRow(0);
        setDataTotal(0);
        // table使用了不自动恢复滚动条模式，需要手动恢复到0
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 0 });
        workerSetMemoryStateData({ data: [] });
        getAllEventListData(session);
    }, [session.deviceId]);

    useEffect(() => {
        const result = getMatchedIndexes();
        setSearchIndexList(result);
        if (result.length < 1) {
            setCurrentShowRow(-1);
            return;
        }
        const selectedMatchIndex = result.findIndex(index => index === currentSelectRow);
        const nextShowRow = selectedMatchIndex >= 0 ? selectedMatchIndex : 0;
        setCurrentShowRow(nextShowRow);
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * result[nextShowRow] });
    }, [searchValues.address, searchValues.eventType, dataSource.length]);

    useEffect(() => {
        const handleDocumentMouseDown = (ev: MouseEvent): void => {
            if (findPanelRef.current?.contains(ev.target as Node)) {
                return;
            }
            setIsFilterMenuOpen(false);
            setIsEventTypeMenuOpen(false);
        };
        document.addEventListener('mousedown', handleDocumentMouseDown);
        return () => {
            document.removeEventListener('mousedown', handleDocumentMouseDown);
        };
    }, []);

    useEffect(() => {
        if (session.leaksWorkerInfo.clickItem === null) {
            return;
        }
        const index = dataSource.findIndex(item => item.id === session.leaksWorkerInfo.clickItem?.id);
        setCurrentSelectRow(index);
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * index });
    }, [session.leaksWorkerInfo.clickItem]);

    useEffect(() => {
        const target = session.pendingEventLocate;
        if (target === null) {
            return;
        }
        if (target.deviceId !== session.deviceId) {
            message.warning(t('eventLocateContextMismatch'));
            runInAction(() => {
                session.pendingEventLocate = null;
            });
            return;
        }
        const targetId = target.eventId;
        const index = dataSource.findIndex(item => item.id === targetId);
        if (index < 0) {
            if (dataTotal !== 0 && dataSource.length >= dataTotal) {
                message.warning(t('eventLocateNotFound'));
                runInAction(() => {
                    session.pendingEventLocate = null;
                });
            }
            return;
        }
        setCurrentSelectRow(index);
        document.querySelector('[data-testid="stateDiagramPanel"]')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
        tableRef.current?.getVirtualBoxDom()?.scrollTo({ top: 32 * index });
        selectEventRow(dataSource[index]);
        runInAction(() => {
            session.pendingEventLocate = null;
        });
    }, [session.pendingEventLocate, session.deviceId, dataSource.length, dataTotal]);

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

    const hasActiveSearch = searchValues.address.trim().length > 0 || searchValues.eventType.trim().length > 0;
    const searchFieldOptions: Array<{ value: EventSearchField; label: string }> = [
        { value: 'address', label: t('eventSearchAddressField') },
        { value: 'eventType', label: t('eventSearchTypeField') },
    ];
    const eventTypeOptions = useMemo(() => {
        return Array.from(new Set(dataSource.map(item => String(item.action ?? '')).filter(Boolean))).sort();
    }, [dataSource]);
    const selectedSearchFields = [
        ...searchFieldOrder,
        ...searchFieldOptions.map(item => item.value).filter(field => !searchFieldOrder.includes(field)),
    ].filter(field => field === searchField || searchValues[field].trim().length > 0 || draftSearchValues[field].trim().length > 0);
    const selectedSearchFieldOptions = selectedSearchFields.map(field => searchFieldOptions.find(item => item.value === field)).filter((item): item is { value: EventSearchField; label: string } => item !== undefined);
    const availableSearchFieldOptions = searchFieldOptions.filter(item => item.value !== searchField && searchValues[item.value].trim().length < 1 && draftSearchValues[item.value].trim().length < 1);

    return <div>
        <FindPanel ref={findPanelRef}>
            <SearchField active={isFilterMenuOpen} onClick={handleSearchFieldClick}>
                <SearchIcon>
                    <SearchOutlined />
                </SearchIcon>
                <SearchTokens>
                    {selectedSearchFieldOptions.map(item => (
                        <ActiveFilterTag
                            key={item.value}
                            menuOpen={item.value === 'eventType' && isEventTypeMenuOpen}
                            reserveTail={availableSearchFieldOptions.length > 0}
                            onClick={(ev): void => {
                                ev.stopPropagation();
                                editSearchField(item.value);
                            }}
                        >
                            <CurrentFilterLabel>{item.label}:</CurrentFilterLabel>
                            {item.value === 'eventType'
                                ? <EventTypePicker>
                                    <EventTypeButton
                                        ref={eventTypeButtonRef}
                                        type="button"
                                        onFocus={(): void => setSearchField(item.value)}
                                        onClick={(ev): void => {
                                            ev.stopPropagation();
                                            openEventTypeMenu();
                                        }}
                                    >
                                        <span>{searchField === item.value ? draftSearchValues[item.value] : searchValues[item.value]}</span>
                                    </EventTypeButton>
                                    {isEventTypeMenuOpen && <EventTypeMenu>
                                        {eventTypeOptions.map(type => (
                                            <EventTypeOption
                                                key={type}
                                                type="button"
                                                active={type === searchValues.eventType}
                                                onClick={(ev): void => {
                                                    ev.stopPropagation();
                                                    setDraftSearchValues(prevValues => ({ ...prevValues, eventType: type }));
                                                    setSearchValues(prevValues => ({ ...prevValues, eventType: type }));
                                                    setSearchFieldOrder(prevOrder => prevOrder.includes('eventType') ? prevOrder : [...prevOrder, 'eventType']);
                                                    setSearchField('eventType');
                                                    setIsEventTypeMenuOpen(false);
                                                }}
                                            >
                                                {type}
                                            </EventTypeOption>
                                        ))}
                                    </EventTypeMenu>}
                                </EventTypePicker>
                                : <EventSearchInput
                                    ref={(element): void => {
                                        searchInputRefs.current[item.value] = element;
                                    }}
                                    value={searchField === item.value ? draftSearchValues[item.value] : searchValues[item.value]}
                                    style={{ width: `${Math.max((searchField === item.value ? draftSearchValues[item.value] : searchValues[item.value]).length + 2, 5)}ch` }}
                                    readOnly={searchField !== item.value}
                                    onFocus={(): void => editSearchField(item.value)}
                                    onChange={(ev): void => handleSearchChange(item.value, ev)}
                                    onBlur={(): void => applySearchField(item.value)}
                                    onKeyDown={(ev): void => {
                                        if (ev.key === 'Enter') {
                                            applySearchField(item.value);
                                            searchInputRefs.current[item.value]?.blur();
                                        }
                                    }}
                                />}
                            <TokenClose onClick={(ev): void => {
                                ev.stopPropagation();
                                setSearchValues(prevValues => ({ ...prevValues, [item.value]: '' }));
                                setDraftSearchValues(prevValues => ({ ...prevValues, [item.value]: '' }));
                                setSearchFieldOrder(prevOrder => prevOrder.filter(field => field !== item.value));
                                if (searchField === item.value) {
                                    setSearchField(null);
                                }
                            }}>
                                <CloseCircleFilled />
                            </TokenClose>
                        </ActiveFilterTag>
                    ))}
                    {(availableSearchFieldOptions.length > 0 || selectedSearchFieldOptions.length < 1) && <SearchTail>
                        {availableSearchFieldOptions.length > 0 && <FilterMenuAnchor>
                            <AddFilterAnchor
                                ref={addFilterAnchorRef}
                                onFocus={() => setIsFilterMenuOpen(true)}
                                onClick={(ev): void => {
                                    ev.stopPropagation();
                                    setIsFilterMenuOpen(true);
                                }}
                                onChange={(ev): void => {
                                    ev.currentTarget.value = '';
                                }}
                                onPaste={handleDefaultSearchPaste}
                            />
                            {selectedSearchFieldOptions.length > 0 && <TailPlaceholder>{t('eventSearchPlaceholder')}</TailPlaceholder>}
                            {isFilterMenuOpen && availableSearchFieldOptions.length > 0 && <FilterMenu>
                                {availableSearchFieldOptions.map(item => (
                                    <EventTypeOption key={item.value} type="button" onClick={(ev): void => {
                                        ev.stopPropagation();
                                        editSearchField(item.value, true);
                                    }}>
                                        <span>{item.label}</span>
                                    </EventTypeOption>
                                ))}
                            </FilterMenu>}
                        </FilterMenuAnchor>}
                        {selectedSearchFieldOptions.length < 1 && <SearchPlaceholder>{t('eventSearchPlaceholder')}</SearchPlaceholder>}
                    </SearchTail>}
                </SearchTokens>
                {hasActiveSearch && <ClearAllButton onClick={(ev): void => {
                    ev.stopPropagation();
                    setSearchValues({ address: '', eventType: '' });
                    setDraftSearchValues({ address: '', eventType: '' });
                    setSearchFieldOrder([]);
                    setSearchField(null);
                }}>
                    <CloseCircleFilled />
                </ClearAllButton>}
            </SearchField>
            <ToolGroup>
                <MatchCount>{hasActiveSearch ? (searchIndexList.length > 0 ? `${currentShowRow + 1}/${searchIndexList.length}` : t('eventSearchNoMatch')) : ''}</MatchCount>
                {hasActiveSearch && searchIndexList.length > 0 && <>
                    <NavButton disabled={!canUpRow} onClick={(ev): void => {
                        ev.stopPropagation();
                        changeScroll();
                    }}>
                        <ArrowUpOutlined />
                    </NavButton>
                    <NavButton disabled={!canDownRow} onClick={(ev): void => {
                        ev.stopPropagation();
                        changeScroll('down');
                    }}>
                        <ArrowDownOutlined />
                    </NavButton>
                </>}
            </ToolGroup>
        </FindPanel>
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
                    selectEventRow(row);
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

const MatchCount = styled.div`
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorSecondary};
    font-size: 12px; line-height: 28px; min-width: 42px; margin-right: 2px;
    text-align: right; white-space: nowrap;
`;

const FindPanel = styled.div`
    position: relative; display: flex; align-items: flex-start; width: 100%;
    min-width: 0; min-height: 32px; margin-bottom: 8px; padding: 1px 0;
`;

const SearchField = styled.div<{ active?: boolean }>`
    position: relative; display: grid; align-items: start; flex: 1 1 auto;
    grid-template-columns: 26px minmax(0, 1fr) auto;
    min-width: 0; width: auto; min-height: 30px; box-sizing: border-box;
    overflow: visible; padding: 2px 0; z-index: 1; border-radius: 3px;
    border: 1px solid ${(props): string => props.active ? props.theme.primaryColor : (props.theme.mode === 'dark' ? '#3c3c3c' : props.theme.borderColorLighter)};
    background: ${(props): string => props.theme.mode === 'dark' ? '#313131' : props.theme.bgColorLighter};

    &:focus-within { border-color: ${(props): string => props.theme.primaryColor}; }
`;

const SearchIcon = styled.span`
    display: inline-flex; align-items: center; justify-content: center; height: 24px;
    color: ${(props): string => props.theme.mode === 'dark' ? '#8a8a8a' : props.theme.textColorTertiary};
    font-size: 13px;
`;

const TokenClose = styled.span`
    position: relative; z-index: 2; display: inline-flex; align-items: center;
    justify-content: center; flex: 0 0 auto; margin-left: 4px;
    color: ${(props): string => props.theme.mode === 'dark' ? '#9aa0a6' : props.theme.textColorTertiary};
    cursor: pointer; font-size: 11px;

    &:hover { color: ${(props): string => props.theme.mode === 'dark' ? '#c7cbd1' : props.theme.textColorPrimary}; }
`;

const ClearAllButton = styled.button`
    position: relative; z-index: 2; display: inline-flex; align-items: center;
    justify-content: center; flex: 0 0 20px; width: 20px; height: 24px;
    margin: 0 4px 0 6px; padding: 0; border: 0;
    color: ${(props): string => props.theme.textColorTertiary};
    background: transparent; cursor: pointer; font-size: 12px;

    &:hover { color: ${(props): string => props.theme.textColorPrimary}; }
`;

const SearchTail = styled.span`
    position: relative; display: inline-flex; align-items: center;
    flex: 0 0 auto; min-width: 42px; margin-right: 2px;
`;

const SearchTokens = styled.span`
    display: flex; align-items: center; flex-wrap: wrap; min-width: 0; gap: 3px 0;
`;

const ActiveFilterTag = styled.div<{ menuOpen?: boolean; reserveTail?: boolean }>`
    display: flex; align-items: center; flex: 0 1 auto; min-width: 0;
    height: 24px; margin-right: 4px; padding: 0 5px 0 7px; border-radius: 3px;
    max-width: ${(props): string => props.reserveTail ? 'calc(100% - 58px)' : 'calc(100% - 28px)'};
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorPrimary};
    background: ${(props): string => props.theme.mode === 'dark' ? '#3f444a' : props.theme.bgColorLight};
    cursor: text;
    overflow: ${(props): string => props.menuOpen ? 'visible' : 'hidden'};
    z-index: ${(props): string => props.menuOpen ? '25' : 'auto'};

`;

const CurrentFilterLabel = styled.span`
    display: inline-flex; align-items: center; flex: 0 0 auto; max-width: 96px; margin-right: 4px;
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorSecondary};
    font-size: 12px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
`;

const EventSearchInput = styled.input`
    flex: 1 1 auto; min-width: 5ch; max-width: 100%; height: 22px;
    padding: 0; border: 0; outline: none; box-shadow: none;
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorPrimary};
    background: transparent; font-size: 12px;

    &::placeholder { color: ${(props): string => props.theme.mode === 'dark' ? '#8a8a8a' : props.theme.textColorTertiary}; }
`;

const EventTypePicker = styled.span`
    position: relative; display: inline-flex; align-items: center;
    flex: 1 1 auto; min-width: 0; max-width: 160px; height: 22px;
`;

const EventTypeButton = styled.button`
    display: inline-flex; align-items: center; justify-content: flex-start;
    flex: 1 1 auto; min-width: 0; width: 100%; height: 22px;
    padding: 0; border: 0; outline: none;
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorPrimary};
    background: ${(props): string => props.theme.mode === 'dark' ? '#3f444a' : props.theme.bgColorLight};
    font-size: 12px; cursor: pointer;

    span { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
`;

const EventTypeMenu = styled.div`
    position: absolute; top: 24px; left: 0; z-index: 30;
    width: 160px; max-height: 220px; padding: 4px 0;
    border: 1px solid ${(props): string => props.theme.mode === 'dark' ? '#454545' : props.theme.borderColorLighter};
    border-radius: 4px;
    background: ${(props): string => props.theme.mode === 'dark' ? '#252526' : props.theme.bgColorCommon};
    box-shadow: ${(props): string => props.theme.mode === 'dark' ? '0 4px 12px rgba(0, 0, 0, 0.36)' : '0 4px 12px rgba(0, 0, 0, 0.12)'};
    overflow: auto;
`;

const EventTypeOption = styled.button<{ active?: boolean }>`
    display: block; width: 100%; height: 28px; padding: 0 10px; border: 0;
    color: ${(props): string => props.active ? props.theme.primaryColor : props.theme.textColorPrimary};
    background: ${(props): string => props.active ? (props.theme.mode === 'dark' ? '#04395e' : props.theme.primaryColorLight5) : 'transparent'};
    cursor: pointer; font-size: 12px; text-align: left;

    &:hover { background: ${(props): string => props.theme.mode === 'dark' ? '#2a2d2e' : props.theme.bgColorLight}; }
`;

const AddFilterAnchor = styled.input`
    display: block; width: 10px; height: 24px; padding: 0; border: 0;
    outline: none; background: transparent; cursor: text;
    caret-color: ${(props): string => props.theme.primaryColor};
    color: transparent; font-size: 12px;
`;

const SearchPlaceholder = styled.span`
    color: ${(props): string => props.theme.mode === 'dark' ? '#8a8a8a' : props.theme.textColorTertiary};
    font-size: 12px; line-height: 24px; pointer-events: none;
`;

const TailPlaceholder = styled(SearchPlaceholder)`
    margin-left: 2px;
`;

const FilterMenuAnchor = styled.span`
    position: relative; display: inline-flex; align-items: center;
    flex: 0 0 auto; width: max-content; max-width: 100%; height: 24px;
`;

const FilterMenu = styled.div`
    position: absolute; top: 26px; left: 0; z-index: 20; width: 180px; padding: 4px 0;
    border: 1px solid ${(props): string => props.theme.mode === 'dark' ? '#454545' : props.theme.borderColorLighter};
    border-radius: 4px;
    background: ${(props): string => props.theme.mode === 'dark' ? '#252526' : props.theme.bgColorCommon};
    box-shadow: ${(props): string => props.theme.mode === 'dark' ? '0 4px 12px rgba(0, 0, 0, 0.36)' : '0 4px 12px rgba(0, 0, 0, 0.12)'};
`;

const ToolGroup = styled.div`
    position: relative; z-index: 2; display: inline-flex; align-items: center;
    justify-content: flex-start; flex: 0 0 auto; height: 28px; padding-left: 12px; gap: 4px;
`;

const NavButton = styled.button<{ disabled?: boolean }>`
    width: 18px; height: 24px; border: 0; border-radius: 2px; padding: 0;
    background: transparent;
    color: ${(props): string => props.disabled ? props.theme.textColorDisabled : (props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorSecondary)};
    cursor: ${(props): string => props.disabled ? 'not-allowed' : 'pointer'};

    &:hover { background: ${(props): string => props.disabled ? 'transparent' : (props.theme.mode === 'dark' ? '#3a3d41' : props.theme.bgColorLight)}; }
`;

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
