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
import { observer } from 'mobx-react';
import React, { useEffect, useMemo, useState } from 'react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { Button, Select, FormItem, Tooltip } from '@insight/lib/components';
import {
    getColumnSearchProps,
    getDefaultColumData,
    getPageData,
    statsSystemViewItems,
    expertSystemViewItems,
    getVisibleStatsSystemViewItems,
    type IQueryCondition,
    SystemViewItem, queryTableDataNameList, type IndexedSystemViewItem, ftraceTypes,
} from './Common';
import { ResizeTable } from '@insight/lib/resize';
import { limitInput, StyledEmpty, GroupCardRankInfosByHost, getRankInfoLabel } from '@insight/lib/utils';
import type { CardMetaData } from '../../entity/data';
import { ChartErrorBoundary } from '../error/ChartErrorBoundary';
import { getTimeOffset } from '../../insight/units/utils';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';
import { HelpIcon } from '@insight/lib/icon';
import { StatsSystemView } from './StatsSystemView';
import { ExpertSystemView, handleAdvisorSelected } from './ExpertSystemView';
import { EventView } from './EventsView';
import { TableDataView } from './TableDataView';
import { Session } from '../../entity/session';
import type { BaseSummaryRowItemType, CardRankInfo } from '../../api/interface';
import { ProjectType } from '../../entity/insight';

export const DETAIL_HEADER_HEIGHT_ETC_PX = 146;
const Container = styled.div`
    width: 100%;
    height: 100%;
    display: flex;
    flex-flow: nowrap;
    background-color: ${(p): string => p.theme.bgColorDark};
    .ant-tree {
        width: 280px;
        height: 100%;
        background-color: ${(p): string => p.theme.contentBackgroundColor};
        overflow: auto;
    }
    .ant-tree-node-selected {
        background-color: var(--grey50) !important;
    }
    .ant-divider-vertical {
        height: 100%;
        border-color: var(--grey80);
    }
`;

const AsideSelectContainer = styled.div`
    display: flex;
    flex-direction: column;
    flex: none;
    padding: 8px 16px;
    margin-right: 8px;
    border-radius: 4px;
    background-color: ${(p): string => p.theme.bgColor};
    max-width: 400px;

    .time-range-info{
        margin-bottom: 8px;
        flex: none;
    }
    .view-select{
        margin-bottom: 8px;
        flex: none;
    }
    .rank-filter{
        margin-bottom: 8px;
        flex: none;
        color: ${(p): string => p.theme.textColorSecondary};
    }
`;

const SelectContentContainer = styled.div`
    flex: 1;
    padding: 8px 16px;
    height: 100%;
    border-radius: 4px;
    overflow: hidden;
    background-color: ${(p): string => p.theme.bgColor};
    .ant-table-wrapper {
        height: 100%;
    }
`;

const AsideSelectList = styled.div`
    flex: 1;
    overflow: auto;
    & .aside-select-item {
        display: flex;
        align-items: center;
        cursor: pointer;
        color: ${(props): string => props.theme.textColorSecondary};
        + .aside-select-item {
            margin-top: 8px;
        }
        &.selected{
            color: ${(props): string => props.theme.primaryColor};
        }
    }
`;

export interface SelectedCardInfo {
    cardId: string;
    dbPath: string;
}

/**
 * 添加服务化视图描述字段接口
 */
export interface ServiceLayers {
    name: string;
    description: string;
}

interface ConditionType<T, K> {
    options: T[];
    value: K;
}

interface HostConditionType extends ConditionType<string, string> {
    cardsMap?: Map<string, CardRankInfo[]>;
}

export interface SelectContentViewProps {
    key: string | number;
    card: SelectedCardInfo;
    session: Session;
    bottomHeight: number;
}

type SelectContentViewComponent<T extends SelectContentViewProps = SelectContentViewProps> = React.FC<T>;

export const DEFAULT_CARD_VALUE = { cardId: '', dbPath: '' };

const findRankCardInfo = (session: Session, card: SelectedCardInfo): CardRankInfo | undefined => {
    return Array.from(session.rankCardInfoMap.values()).find((cardRankInfo) => {
        return cardRankInfo.rankInfo.rankId === card.cardId && cardRankInfo.dbPath === card.dbPath;
    });
};

const isRequestCardTypeMismatch = (session: Session, card: SelectedCardInfo, isFtrace: boolean): boolean => {
    const rankCardInfo = findRankCardInfo(session, card);
    if (isFtrace) {
        return rankCardInfo?.isFtrace !== true;
    }
    return rankCardInfo?.isFtrace === true;
};

export const SystemView = observer((props: any) => {
    const [viewOption, setViewOption] = useState(0);
    const [key, setKey] = useState(0);
    const isFtraceStatsItem = viewOption === 0 && ftraceTypes.includes(statsSystemViewItems[key]?.name ?? '');
    // eslint-disable-next-line camelcase
    const SelectContent = useMemo(() => {
        // 第四个tab的特殊逻辑
        if (viewOption === 0 && key >= statsSystemViewItems.length) {
            return null;
        }
        return contentList[viewOption][key];
    }, [viewOption, key]);
    const [conditions, setConditions] = useState<SelectedCardInfo>(DEFAULT_CARD_VALUE);
    const handleChange = (card: SelectedCardInfo): void => {
        setConditions(card);
    };
    const handleViewChange = (value: number): void => {
        setViewOption(value);
        setKey(0);
    };
    useEffect(() => {
        if (props.session.showEvent !== undefined) {
            setViewOption(2);
            setKey(0);
        }
    }, [props.session.showEvent]);
    return (<Container>
        <AsideSelectContainer>
            {viewOption !== 3 && (<TimeRangeInfo session={props.session}></TimeRangeInfo>)}
            <ViewSelect viewOption={viewOption} handleViewChange={handleViewChange}/>
            {viewOption !== 2 && (
                <RankFilter
                    session={props.session}
                    viewOption={viewOption}
                    handleChange={handleChange}
                    isFtraceRankOnly={isFtraceStatsItem}
                    excludeFtraceRank={viewOption === 0 && !isFtraceStatsItem}
                />
            )}
            <SelectList viewOption={viewOption} selectKey={key} setKey={setKey} card={conditions} session={props.session}></SelectList>
        </AsideSelectContainer>
        <ChartErrorBoundary>
            <SelectContentContainer>
                {viewOption === 0 && key >= statsSystemViewItems.length
                    ? <TableDataView key={key - statsSystemViewItems.length} selectKey={key - statsSystemViewItems.length} card={conditions} session={props.session}
                        bottomHeight={props.bottomHeight}></TableDataView>
                    : SelectContent && (<SelectContent key={key} card={conditions} session={props.session}
                        bottomHeight={props.bottomHeight}></SelectContent>)}
            </SelectContentContainer>
        </ChartErrorBoundary>
    </Container>);
});

const TimeRangeInfo = observer((props: { session: Session }) => {
    const { session } = props;
    const { t } = useTranslation('timeline');
    return (
        session.isTimeAnalysisMode && session.timeAnalysisRange
            ? <div className={'time-range-info'}>
                {t('contextMenu.Time Filter')}
                {': '}
                <strong>{getDetailTimeDisplay(session.timeAnalysisRange[0])}</strong>
                {' '}
                {t('contextMenu.to')}
                {' '}
                <strong>{getDetailTimeDisplay(session.timeAnalysisRange[1])}</strong>
            </div>
            : <></>
    );
});

const ViewSelect = observer((props: any) => {
    const { viewOption, handleViewChange } = props;
    const { t } = useTranslation('timeline', { keyPrefix: 'systemView' });
    const options = [{ label: t('Stats System View'), value: 0 }, { label: t('Expert System View'), value: 1 }, { label: t('Events View'), value: 2 }];
    return (
        <div className={'view-select'}>
            <Select id={'select-system-view'} width={'100%'} value={viewOption} onChange={handleViewChange} options={options}/>
        </div>
    );
});

// eslint-disable-next-line max-lines-per-function
export const RankFilter = observer((props: {
    session: Session;
    viewOption?: number;
    handleChange: (v: SelectedCardInfo) => void;
    defaultRankId?: string;
    isFtraceRankOnly?: boolean;
    excludeFtraceRank?: boolean;
}): JSX.Element => {
    const [rankCondition, setRankCondition] = useState<ConditionType<CardRankInfo, number | undefined>>({ options: [], value: undefined });
    const [hostCondition, setHostCondition] = useState<HostConditionType>({ options: [], value: '' });
    const { t } = useTranslation('timeline');
    const excludeFtraceRank = props.excludeFtraceRank ?? false;

    useEffect(() => {
        const cardList: CardRankInfo[] = [];
        for (const v of props.session.rankCardInfoMap.values()) {
            if (v.rankInfo.rankId.endsWith('Host')) {
                continue;
            }
            if (props.isFtraceRankOnly && v.isFtrace !== true) {
                continue;
            }
            if (excludeFtraceRank && v.isFtrace === true) {
                continue;
            }
            cardList.push(v);
        }
        if (props.isFtraceRankOnly) {
            setHostCondition({ options: [], value: '', cardsMap: new Map([['', cardList]]) });
            return;
        }
        const { hosts, cardsMap }: { hosts: string[]; cardsMap: Map<string, CardRankInfo[]> } = GroupCardRankInfosByHost(cardList);

        // 如果有 defaultRankId，找到对应的 host
        let initialHost = hosts[0] ?? '';
        if (props.defaultRankId !== undefined && props.defaultRankId !== 'ALL') {
            for (const [host, cards] of cardsMap.entries()) {
                if (cards.some(card => card.rankInfo.rankId === props.defaultRankId)) {
                    initialHost = host;
                    break;
                }
            }
        }
        setHostCondition({ options: hosts, value: initialHost, cardsMap });
    }, [props.session.rankCardInfoMap.size, props.defaultRankId, props.isFtraceRankOnly, excludeFtraceRank]);

    useEffect(() => {
        const rankOptions = hostCondition.cardsMap?.get(hostCondition.value) ?? [];

        // 如果有 defaultRankId，找到对应的 index
        let initialIndex = rankOptions.length > 0 ? 0 : undefined;
        if (props.defaultRankId !== undefined && props.defaultRankId !== 'ALL') {
            const foundIndex = rankOptions.findIndex(card => card.rankInfo.rankId === props.defaultRankId);
            if (foundIndex !== -1) {
                initialIndex = foundIndex;
            }
        }
        setRankCondition({ options: rankOptions, value: initialIndex });
    }, [hostCondition, props.defaultRankId]);

    useEffect(() => {
        if (rankCondition.value === undefined) {
            props.handleChange({ cardId: '', dbPath: '' });
            return;
        }
        const cardRankInfo = rankCondition.options[rankCondition.value];
        props.handleChange({ cardId: cardRankInfo.rankInfo.rankId, dbPath: cardRankInfo.dbPath });
    }, [rankCondition]);

    useEffect(() => {
        limitInput();
    }, []);

    const onRankIdChanged = (value: number): void => {
        setRankCondition({ ...rankCondition, value });
    };
    return (<div className={'rank-filter'} >
        {!props.isFtraceRankOnly && hostCondition.options.length > 0
            ? <FormItem label={t('Host')} contentStyle={{ flex: 1, minWidth: 0 }}>
                <Select
                    value={hostCondition.value}
                    width={'100%'}
                    onChange={(value: string): void => setHostCondition({ ...hostCondition, value })}
                    options={hostCondition.options.map((host) => ({ value: host, label: host }))}
                />
            </FormItem>
            : <></>
        }
        <FormItem label={t('Rank ID')} contentStyle={{ flex: 1, minWidth: 0 }}>
            <Select
                value={rankCondition.value}
                width={'100%'}
                onChange={onRankIdChanged}
                options={rankCondition.options.map((card, index) => {
                    return {
                        value: index,
                        label: getRankInfoLabel(card.rankInfo),
                    };
                })}
                showSearch={true}
            />
        </FormItem>
    </div>);
});
/**
 * 国际化-中文
 */
const LANGUAGE_ZH = 'zhCN';

const SelectList = observer((props: { session: Session; viewOption: number; selectKey: number; setKey: (v: number) => void; card: SelectedCardInfo}) => {
    const [selectedKey, setSelectedKey] = useState(0);
    const [systemViewItems, setSystemViewItems] = useState<SystemViewItem[]>([]);
    const { t } = useTranslation('timeline', { keyPrefix: 'systemView' });
    const handleClick = (key: number): void => {
        props.setKey(key);
        setSelectedKey(key);
    };
    const params = useMemo(() => {
        return { rankId: props.card.cardId, dbPath: props.card.dbPath, isZh: props.session.language === LANGUAGE_ZH };
    }, [props.card, props.session.language]);
    useEffect(() => {
        setSelectedKey(props.selectKey);
    }, [props.selectKey]);
    const displayItems = useMemo(() => {
        if (props.viewOption !== 0) {
            return systemViewItems.map((item, index) => ({ ...item, originIndex: index }));
        }
        // 服务化只展示tableData/nameList接口返回的视图名称
        if (systemViewItems.length > statsSystemViewItems.length) {
            return systemViewItems.map((item, index) => ({ ...item, originIndex: index })).slice(statsSystemViewItems.length);
        }
        return getVisibleStatsSystemViewItems(
            systemViewItems,
            props.session.hasFtraceData,
            props.session.hasNonFtraceData,
        );
    }, [props.viewOption, props.session.hasFtraceData, props.session.hasNonFtraceData, systemViewItems]);
    useEffect(() => {
        switch (props.viewOption) {
            case 0: {
                if (params.rankId !== undefined && params.rankId !== '') {
                    queryTableDataNameList(params).then((res) => {
                        // 获取服务视图名称和描述数据
                        const layers = res.layers as SystemViewItem[];
                        if (layers !== undefined && layers.length > 0) {
                            const merged = Array.from(
                                new Map([...statsSystemViewItems, ...layers].map(item => [item.name, item])).values(),
                            );
                            setSystemViewItems(merged);
                        } else {
                            setSystemViewItems(statsSystemViewItems);
                        }
                    }).catch(() => {
                        setSystemViewItems(statsSystemViewItems);
                    });
                } else {
                    setSystemViewItems(statsSystemViewItems);
                }
                break;
            }
            case 1:
                setSystemViewItems(expertSystemViewItems);
                break;
            case 2:
                setSystemViewItems([]);
                break;
            default:
                break;
        }
    }, [props.viewOption, params, props.session.language]);
    useEffect(() => {
        if (props.viewOption !== 0 || displayItems.length === 0) {
            return;
        }
        if (!displayItems.some(item => item.originIndex === props.selectKey)) {
            props.setKey(displayItems[0].originIndex);
            setSelectedKey(displayItems[0].originIndex);
        }
    }, [props.viewOption, props.selectKey, props.setKey, displayItems]);
    const renderItem = (item: IndexedSystemViewItem): JSX.Element => {
        const isDynamicItem = item.originIndex >= statsSystemViewItems.length;
        return (<div
            className={`aside-select-item ${selectedKey === item.originIndex ? 'selected' : ''}`}
            key={item.originIndex}
            onClick={(): void => handleClick(item.originIndex)}
        >
            {isDynamicItem
                ? <Tooltip title={item.description}>
                    <div>{item.name}</div>
                </Tooltip>
                : <>
                    <div>{t(item.name)}</div>
                    {
                        item.tips !== undefined &&
                            <Tooltip title={t(item.tips)} placement={'topLeft'}>
                                <HelpIcon style={{ cursor: 'pointer', marginLeft: 4 }} height={20} width={20} />
                            </Tooltip>
                    }
                </>}
        </div>);
    };
    return (<AsideSelectList>
        {
            displayItems.map(renderItem)
        }
    </AsideSelectList>
    );
});

export interface BaseSummaryProps extends SelectContentViewProps {
    layerType?: string;
    request: (...rest: any[]) => any;
    isStats?: boolean;
    isFtrace?: boolean;
    isTrace?: boolean;
    columns: any;
}

// eslint-disable-next-line max-lines-per-function
export const BaseSummary = observer((props: BaseSummaryProps) => {
    const isStats = props.isStats as boolean;
    const isFtrace = props.isFtrace as boolean;
    const isTrace = props.isTrace as boolean;
    const getDefaultSorter = (): {field: string;order: string} => {
        if (isStats) {
            return { field: 'totalTime', order: 'descend' };
        }
        if (isTrace) {
            return { field: 'startTime', order: 'ascend' };
        }
        return { field: 'duration', order: 'descend' };
    };
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const defaultSorter = getDefaultSorter();
    const [dataSource, setDataSource] = useState<any[]>([]);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [isLoading, setLoading] = useState(false);
    const [searchText, setSearchText] = useState('');
    const [searchedColumn, setSearchedColumn] = useState('');
    const [rowData, setRowData] = useState<Partial<BaseSummaryRowItemType>>({});
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });
    const status = props.session.units.find((unit: any) => (unit.metadata as CardMetaData).cardId === props.card.cardId)?.phase;
    let columns = props.columns?.map((col: any) => ({
        ...col,
        title: t(col.title),
    }));

    if (isStats || isTrace) {
        columns = [{
            title: t('Name'),
            dataIndex: 'name',
            ...getDefaultColumData('name'),
            ...getColumnSearchProps({ dataIndex: 'name', setSearchText, searchText, setSearchedColumn, searchedColumn }),
        }, ...columns];
    } else if (!isFtrace) {
        columns = [...columns, {
            title: t('Click To Timeline'),
            dataIndex: 'click',
            key: 'click',
            ellipsis: true,
            render: (_: any, record: any): JSX.Element => (<Button type="link"
                onClick={(): void => {
                    setRowData({ name: record.name ?? record.originOptimizer, ...record });
                }}>{t('Click')}</Button>),
        }];
    }

    const updateData = async(searchName: string, pages: any, sorters: {field: string;order: string}, prop: BaseSummaryProps): Promise<void> => {
        const _isStats = prop.isStats as boolean;
        const _isFtrace = prop.isFtrace as boolean;
        const _isTrace = prop.isTrace as boolean;
        const targetInfo = props.session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === props.card?.cardId);
        if (props.card === undefined || props.card.cardId === '' || targetInfo?.projectType === ProjectType.IE) {
            setDataSource([]);
            setPage(defaultPage);
            return;
        }
        if (isRequestCardTypeMismatch(prop.session, prop.card, _isFtrace)) {
            setDataSource([]);
            setPage(defaultPage);
            return;
        }
        setLoading(true);
        let startTime = prop.session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = prop.session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(prop.session, { cardId: prop.card.cardId });
        const sortedField = sorters.field === 'startTimeLabel' ? 'startTime' : sorters.field;
        let params: IQueryCondition = {
            rankId: prop.card.cardId,
            dbPath: prop.card.dbPath,
            pageSize: pages.pageSize,
            current: pages.current,
            orderBy: sorters.order ? sortedField : defaultSorter.field,
            order: sorters.order ?? defaultSorter.order,
            startTime: Math.floor(startTime + timestampOffset),
            endTime: Math.ceil(endTime + timestampOffset),
        };
        if (_isStats || _isTrace) {
            params = { isQueryTotal: true, layer: prop.layerType, searchName, ...params };
        }
        if (_isFtrace) {
            params = { layer: prop.layerType, ...params };
        }

        try {
            const res = await props.request(params);
            if (_isStats) {
                setDataSource(res.systemViewDetails);
            } else if (_isTrace) {
                const data = res.systemViewDetails.map((item: any) => ({
                    ...item,
                    startTimeLabel: getDetailTimeDisplay(item.startTime - timestampOffset),
                }));
                setDataSource(data);
            } else {
                const timestampoffset = getTimeOffset(props.session, props.card);
                const dbPath = res.dbPath;
                const data = res.data.map((item: any) => {
                    item.startTimeLabel = getDetailTimeDisplay(item.startTime - timestampoffset);
                    item.dbPath = dbPath;
                    return item;
                });
                setDataSource(data);
            }
            setPage({ ...page, total: res.count });
        } catch (err) {
            setDataSource([]);
            setPage(defaultPage);
        } finally {
            setLoading(false);
        }
    };
    useEffect(() => {
        if (status === 'download') {
            updateData(searchText, page, sorter, props);
        }
    }, [sorter, props.card.cardId, status, props.session.timeAnalysisRange]);
    useEffect(() => {
        if (rowData.name === null || rowData.name === undefined) {
            return;
        }
        handleAdvisorSelected(rowData as BaseSummaryRowItemType, props);
    }, [rowData]);

    return (
        (status === 'download' && props.card !== undefined && props.card.cardId !== '')
            ? <ResizeTable
                onChange={(pagination: any, filters: any, nwSorter: any): void => {
                    setSorter(nwSorter);
                }}
                rowClassName={(record: any): string => {
                    return record.id !== undefined && record.id === rowData.id ? 'selected-row' : 'click-able';
                }}
                pagination={getPageData(page, setPage)}
                dataSource={dataSource}
                columns={columns}
                size="small"
                scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
                loading = {isLoading}/>
            : <div style={{ display: 'flex', height: '100%' }}>
                <StyledEmpty style={{ margin: 'auto' }}/>
            </div>
    );
});

const contentList: SelectContentViewComponent[][] = [StatsSystemView, ExpertSystemView, [EventView]];
