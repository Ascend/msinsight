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
import styled from '@emotion/styled';
import _, { debounce } from 'lodash';
import { Button, Checkbox, Input, Tooltip } from '@insight/lib/components';
import { observer } from 'mobx-react';
import React, { type ChangeEvent, useEffect, useRef, useState } from 'react';
import { LinkIcon } from '@insight/lib/icon';
import type { SelectedDataType, Session } from '../entity/session';
import { CustomButton } from './base/StyledButton';
import { useTranslation } from 'react-i18next';
import { StyledEmpty } from './base/StyledEmpty';
import { action, runInAction } from 'mobx';
import type { InsightUnit, LinkLine, LinkLines } from '../entity/insight';
import { CardUnit, ProcessUnit } from '../insight/units/AscendUnit';
import { customDebounce } from '../utils/customDebounce';
import { getTimeOffset } from '../insight/units/utils';
import { CardMetaData, type HostMetaData, ProcessMetaData, ThreadMetaData } from '../entity/data';
import i18n from '@insight/lib/i18n';
import { message, Spin } from 'antd';
import connector from '../connection/index';
import { getUnitFlows } from '../api/request';
import { FlowItem } from '../api/interface';
import { isNotEmpty } from '@insight/lib';

const FilterContainer = styled.div`
    display: flex;
    flex-direction: column;
    padding: 10px;
`;

const FilterList = styled.div`
    min-height: 100px;
    max-height: 200px;
    overflow-y: scroll;
`;

const FilterButtonLine = styled.div`
    display: flex;
    justify-content: space-evenly;
    align-items: center;
    margin-top: 14px;
`;

interface FilterItemProps {
    category: string;
    checkedCategories: string[];
    setCheckedCategories: React.Dispatch<React.SetStateAction<string[]>>;
    session: Session;
}

const categoryMap: { [key: string]: string } = { MsTx: 'MSTX' };

const FilterItem: React.FC<FilterItemProps> = observer(({ category, checkedCategories, setCheckedCategories, session }) => {
    const isChecked = checkedCategories.includes(category);
    if ((!session.isSimulation && checkedCategories.length >= 10) && !isChecked) {
        return (
            <p style={{ marginBottom: 0 }}>
                <Checkbox
                    checked={false}
                    onChange={(): void => {
                        runInAction(() => {
                            session.drawLineMode = 'all';
                        });
                        message.warning(i18n.t('Line Warning', { ns: 'timeline' }));
                    }}>
                    {categoryMap[category] ?? category}
                </Checkbox>
            </p>
        );
    }
    return (
        <p style={{ marginBottom: 0 }}>
            <Checkbox
                checked={isChecked}
                onChange={(): void => {
                    setCheckedCategories(prev => isChecked ? prev.filter(cat => cat !== category) : prev.concat(category));
                }}>
                {categoryMap[category] ?? category}
            </Checkbox>
        </p>
    );
});

const getCardUnits = (units: InsightUnit[]): InsightUnit[] => {
    return units.flatMap(unit => {
        const res: InsightUnit[] = [];
        if (unit instanceof CardUnit) {
            res.push(unit);
        };
        if (unit.children) {
            res.push(...getCardUnits(unit.children));
        }
        return res;
    });
};

/**
 * 找到 host unit 下的打开的 ProcessUnit(相当于 host 下的卡)
 * @param unit
 */
const getHostChildUnitCardInfos = (unit: InsightUnit): Array<{ cardId: string; dbPath: string }> => {
    if (!Array.isArray(unit.children) || unit.children.length <= 0) {
        return [];
    }
    return unit.children.reduce<Array<{ cardId: string; dbPath: string }>>((openedProcessUnitCardInfos, unit) => {
        if (unit instanceof ProcessUnit && unit.isExpanded) { // 如果 ProcessUnit 是打开的，则应该显示它所代表的卡的连线数据
            const { cardId, dbPath } = unit.metadata;
            openedProcessUnitCardInfos.push({ cardId, dbPath });
        }
        return openedProcessUnitCardInfos;
    }, []);
};

export interface DataBlock {
    pid: string;
    tid: string;
    timestamp: number;
    depth: number;
    height?: number;
    rankId?: string;
    duration?: number;
}
export interface FlowEvent {
    category: string;
    from: DataBlock;
    to: DataBlock;
    cardId: string;
}
interface CategoryEvents {
    flowDetailList: Array<{
        category: string;
        from: DataBlock;
        to: DataBlock;
    }>;
}

type FetchLinkLines = (session: Session) => Promise<CategoryEvents['flowDetailList']>;
type UseFetchLinkLines = Map<string, FetchLinkLines>;

const getLockRangeMetaList = (session: Session, cardId: string | undefined): any => {
    return session.lockUnit.map(selectUnit => {
        const { threadId, processId, metaType } = selectUnit?.metadata as ThreadMetaData ?? {};
        return {
            tid: threadId,
            pid: processId,
            metaType,
            rankId: cardId,
        };
    });
};

interface QueryFlowLinesConfig extends Pick<ProcessMetaData, 'dataSource' | 'cardId' | 'dbPath'> {
    host: string;
    category: string;
    timestampOffset: number;
    domainStart: number;
    domainEnd: number;
    timePerPx: number;
}

/**
 * 查询一张卡的连线
 * @param viewedCardIdSet
 * @param session
 * @param config
 */
const fetchLinkLineForCard = async (viewedCardIdSet: Set<string>, session: Session,
    config: QueryFlowLinesConfig): Promise<CategoryEvents['flowDetailList']> => {
    const { host, dataSource, cardId, dbPath, category, timestampOffset, domainStart, domainEnd, timePerPx } = config;
    // 如果不在可视范围内就不查询
    if (!viewedCardIdSet.has(cardId)) {
        return [];
    }
    const start = Math.floor(domainStart + timestampOffset);
    const end = Math.ceil(domainEnd + timestampOffset);
    const metadataList = getLockRangeMetaList(session, cardId);
    const lockStartTime = session.lockRange === undefined ? 0 : Math.floor(session.lockRange[0] + timestampOffset);
    const lockEndTime = session.lockRange === undefined ? 0 : Math.ceil(session.lockRange[1] + timestampOffset);
    const params = {
        rankId: cardId,
        dbPath,
        startTime: start,
        endTime: end,
        category,
        timePerPx,
        isSimulation: session.isSimulation,
        host,
        metadataList,
        lockStartTime,
        lockEndTime,
    };

    try {
        const res = await window.request(dataSource, { command: 'flow/categoryEvents', params });
        return (res as CategoryEvents).flowDetailList.map(data => ({ ...data, cardId }));
    } catch (e) {
        return [];
    }
};
/**
 * 查询 host 下打开的卡的连线
 * @param unit
 * @param viewedCardIdSet
 * @param session
 * @param config
 */
const queryLinkLinesForHostCards = async (unit: InsightUnit, viewedCardIdSet: Set<string>, session: Session, config: Omit<QueryFlowLinesConfig, 'cardId' | 'dbPath'>):
Promise<CategoryEvents['flowDetailList']> => {
    const hostProcessCardInfos = getHostChildUnitCardInfos(unit).filter(({ cardId }) => viewedCardIdSet.has(cardId));
    const chunkedList = _.chunk(hostProcessCardInfos, 8); // 8个为一组分组
    let res: CategoryEvents['flowDetailList'] = [];
    for (const batch of chunkedList) {
        const results = await Promise.all(batch.map(({ cardId, dbPath }) => fetchLinkLineForCard(viewedCardIdSet, session, {
            ...config,
            cardId,
            dbPath,
        })));
        res = res.concat(results.flat());
    }
    return res;
};

/**
 * 去除重复的连线对象
 * @param arr
 */
function uniqueLinkLines(arr: CategoryEvents['flowDetailList']): CategoryEvents['flowDetailList'] {
    const uniqueLinkLineMap: Map<string, CategoryEvents['flowDetailList'][number]> = new Map();
    const generateKey = (obj: CategoryEvents['flowDetailList'][number]): string => {
        return `${obj.category}_${obj.from.timestamp}/${obj.from.pid}-${obj.from.tid}-${obj.from.depth}_${obj.to.timestamp}/${obj.to.pid}-${obj.to.tid}-${obj.to.depth}`;
    };
    arr.forEach(obj => {
        const key = generateKey(obj);
        if (!uniqueLinkLineMap.has(key)) {
            uniqueLinkLineMap.set(key, obj);
        }
    });
    return [...uniqueLinkLineMap.values()];
}

const useFetchLinkLines = (displayCategories: string[], viewedCardIdSet: Set<string>, ridLineType: string): UseFetchLinkLines => React.useMemo(() => {
    const allAisplayCategories = [...displayCategories];
    if (ridLineType.length > 0 && !allAisplayCategories.includes(ridLineType)) {
        allAisplayCategories.push(ridLineType);
    }
    return new Map(
        allAisplayCategories.map(category => [
            category,
            customDebounce(async (session: Session): Promise<CategoryEvents['flowDetailList']> => {
                const { domainStart, domainEnd } = session.domainRange;
                const { domain: { timePerPx } } = session;
                let res: CategoryEvents['flowDetailList'] = [];
                for (const unit of getCardUnits(session.units)) {
                    if (!unit.isExpanded) {
                        continue;
                    }
                    const metadata = unit.metadata as ProcessMetaData;
                    const timestampOffset = getTimeOffset(session, metadata);
                    const host = (unit.parent?.metadata as HostMetaData)?.host ?? '';
                    const { dataSource, cardId, dbPath } = metadata;
                    const config = {
                        dataSource,
                        host,
                        category,
                        timestampOffset,
                        domainStart,
                        domainEnd,
                        timePerPx,
                    };
                    if (cardId === undefined) { continue; }
                    let cardLinkLines: CategoryEvents['flowDetailList'] = [];
                    if (cardId?.endsWith('Host')) {
                        cardLinkLines = await queryLinkLinesForHostCards(unit, viewedCardIdSet, session, config);
                    } else {
                        cardLinkLines = await fetchLinkLineForCard(viewedCardIdSet, session, { ...config, cardId, dbPath });
                    }
                    res = res.concat(cardLinkLines);
                }
                return uniqueLinkLines(res);
            }),
        ]),
    );
}, [displayCategories, viewedCardIdSet, ridLineType]);

const useGetCategories = (session: Session, isSuspend: boolean): {categories: string[]; loading: boolean} => {
    const [categories, setCategories] = React.useState<string[]>([]);
    const [loading, setLoading] = React.useState<boolean>(false);
    const cardIdsRef = React.useRef<string>();
    React.useEffect(() => {
        if (!isSuspend) {
            return;
        }
        const cardUnits = getCardUnits(session.units);
        const cardUnitsParsed = cardUnits.filter(cardUnit => cardUnit.phase === 'download');
        const parsedCardIdsString = cardUnitsParsed.map((unit) => (unit.metadata as CardMetaData).cardId).toString();

        if (parsedCardIdsString === cardIdsRef.current) { return; }
        cardIdsRef.current = parsedCardIdsString;
        const fetchList: Array<Promise<{ category: string[] }>> = [];
        for (const unit of cardUnitsParsed) {
            const { dataSource, cardId, dbPath } = unit.metadata as CardMetaData;
            fetchList.push(window.request(dataSource, { command: 'flow/categoryList', params: { rankId: cardId, dbPath } }));
        }
        setLoading(true);
        Promise.all(fetchList).then((results) => {
            const curCategories = new Set<string>();
            results.forEach(({ category }) => {
                category.forEach((cat) => {
                    curCategories.add(cat);
                });
            });
            setCategories([...curCategories]);
            runInAction(() => {
                curCategories.forEach((category) => {
                    session.linkLines[category] = undefined;
                });
            });
        }).finally(() => {
            setLoading(false);
        });
    }, [isSuspend]);

    React.useEffect(() => {
        cardIdsRef.current = '';
        setCategories([]);
    }, [session.doReset]);

    return { categories, loading };
};

const updateSessionLineData = (checkedCategories: string[], fetchLinkLinesMap: Map<string, FetchLinkLines>, session: Session): any => {
    return async () => {
        const newLines: LinkLines = {};
        const results = await Promise.all(checkedCategories.map((category) => {
            return fetchLinkLinesMap.get(category)?.(session);
        }));
        for (let i = 0; i < results.length; ++i) {
            const res = results[i];
            if (res === undefined) {
                return;
            }
            newLines[checkedCategories[i]] = res;
        }
        newLines[session.ridLineType] = await fetchLinkLinesMap.get(session.ridLineType)?.(session);
        runInAction(() => {
            session.singleLinkLine = {};
            session.linkLines = newLines;
            session.renderTrigger = !session.renderTrigger;
            session.linkLineCategories = checkedCategories;
        });
    };
};

/**
 * 查询单条连线
 */
const getSingleFlow = async (session: Session): Promise<void> => {
    if (session.drawLineMode === 'single' && isNotEmpty(session.singleLinkLine) && isNotEmpty(session.selectedData)) {
        const {
            startTime,
            duration,
            threadId,
            cardId,
            dbPath,
            id,
            processId,
            timestamp,
            metaType,
        } = session.selectedData as SelectedDataType;

        if (cardId === undefined || id === undefined) {
            return;
        }
        const timestampOffset = getTimeOffset(session, { cardId, processId });
        const startTimeWithOffset = timestampOffset + startTime;
        const raw = await getUnitFlows({
            dbPath,
            rankId: cardId,
            tid: threadId,
            pid: processId,
            startTime: timestamp ?? startTimeWithOffset,
            endTime: (timestamp ?? startTimeWithOffset) + duration,
            id,
            metaType,
            isSimulation: session.isSimulation,
        });
        const categoryFlowEvents = raw.unitAllFlows as FlowItem[] ?? [];
        const newLines: LinkLines = {};
        for (const categoryFlowEvent of categoryFlowEvents) {
            const cat = categoryFlowEvent.cat;
            const singleCatLinkLine: LinkLine = [];
            for (const flow of categoryFlowEvent.flows) {
                const singleLine: Record<string, unknown> = {
                    category: flow.cat,
                    cardId,
                    from: flow.from,
                    to: flow.to,
                };
                singleCatLinkLine.push(singleLine);
            }
            newLines[cat] = singleCatLinkLine;
        }
        runInAction(() => {
            session.singleLinkLine = newLines;
            session.renderTrigger = !session.renderTrigger;
        });
    }
};

const LinkLineFilterBody = observer(({ session, isSuspend, checkedCategories, setCheckedCategories }:
{ session: Session; isSuspend: boolean; checkedCategories: string[]; setCheckedCategories: React.Dispatch<React.SetStateAction<string[]>> }): JSX.Element => {
    let { categories: displayCategories, loading } = useGetCategories(session, isSuspend);
    const [inputValue, setInput] = React.useState<string>();
    const isEmptyData = displayCategories.length === 0;
    const onInputChange = action((e: ChangeEvent<HTMLInputElement>): void => {
        const inputContent = e.target.value;
        const trimmedValue = inputContent.trim();
        setInput(trimmedValue);
    });
    if (inputValue !== undefined && inputValue.length > 0) {
        displayCategories = displayCategories.filter(str => str.toLowerCase().includes(inputValue.toLowerCase()));
    }
    React.useEffect(() => {
        setCheckedCategories([]);
        setInput('');
    }, [session.doReset]);
    return (
        <Spin spinning={loading} delay={400}>
            <FilterContainer>
                <Input size="middle" allowClear value={inputValue} onChange={onInputChange} style={{ width: '100%', marginBottom: 10 }}></Input>
                <FilterList>
                    {isEmptyData
                        ? <StyledEmpty />
                        : displayCategories.map((category, index) => <FilterItem
                            key={index}
                            session={session}
                            category={category}
                            checkedCategories={checkedCategories}
                            setCheckedCategories={setCheckedCategories}
                        />)}
                </FilterList>
                {!isEmptyData && <FilterButtonLine>
                    <Button type={'primary'} onClick={(): void => {
                        if (!session.isSimulation && displayCategories.length > 10) {
                            message.warning(i18n.t('Line Warning', { ns: 'timeline' }));
                            return;
                        }
                        session.drawLineMode = 'all';
                        setCheckedCategories([...displayCategories]);
                    }}>
                        {i18n.t('timeline:All')}
                    </Button>
                    <Button onClick={((): void => setCheckedCategories([]))}>
                        {i18n.t('timeline:None')}
                    </Button>
                </FilterButtonLine>}
            </FilterContainer>
        </Spin>
    );
});

export const FilterLinkLine = observer(({ session }: { session: Session}): JSX.Element | null => {
    const { t } = useTranslation();
    const [customButtonProps, updateCustomButtonProps] = useState({
        isEmphasize: false,
        isSuspend: false,
    });
    const [checkedCategories, setCheckedCategories] = React.useState<string[]>([]);
    const fetchLinkLinesMap = useFetchLinkLines(checkedCategories, session.viewedExpandedCardIdSet, session.ridLineType);
    const updateLinkLines = React.useCallback(updateSessionLineData(checkedCategories, fetchLinkLinesMap, session),
        [checkedCategories, session.viewedExpandedCardIdSet, session.ridLineType]);
    const updateLineData = (): void => {
        updateLinkLines();
    };
    const [filterLinkLineDisabled, setFilterLinkLineDisabled] = useState(true);
    connector.addListener('updateCategory', (e) => {
        setFilterLinkLineDisabled(e.data.body.data);
    });
    const dependencyParam = [
        session.domainRange.domainStart,
        session.domainRange.domainEnd,
        checkedCategories,
        session?.unitsConfig.offsetConfig.timestampOffset,
        session.viewedExpandedCardIdSet,
        session.ridLineType,
        session.drawLineMode,
        session.shouldRefetchLines,
    ];
    React.useEffect(() => {
        if (session.drawLineMode !== 'single') {
            updateLineData();
        }
    }, dependencyParam);
    // tooltip显隐控制悬浮效果
    const onTooltipVisibleChange = (visible: boolean): void => {
        updateCustomButtonProps({ ...customButtonProps, isSuspend: visible });
    };
    const ref = useRef<HTMLButtonElement>(null);

    useEffect(() => {
        const fn = debounce(getSingleFlow, 300);
        fn(session);

        return () => fn.cancel();
    }, [
        session.domainRange.domainStart,
        session.domainRange.domainEnd,
        session?.unitsConfig.offsetConfig.timestampOffset,
    ]);
    return (
        <Tooltip overlayStyle={{ maxWidth: 1000 }}
            title={<LinkLineFilterBody
                session={session}
                isSuspend={customButtonProps.isSuspend}
                checkedCategories={checkedCategories}
                setCheckedCategories={setCheckedCategories}
            />}
            trigger="click"
            onOpenChange={onTooltipVisibleChange}
            overlayInnerStyle={{ borderRadius: 2 }}
            overlayClassName={'insight-category-search-overlay'} align={{ offset: [-8, 3] }}>
            <CustomButton data-testid={'tool-flow'} disabled={filterLinkLineDisabled} tooltip={t('tooltip:linker')} icon={LinkIcon as any} { ...customButtonProps } ref={ref}/>
        </Tooltip>
    );
});
