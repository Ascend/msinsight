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

import React, { useEffect } from 'react';
import { observer } from 'mobx-react-lite';
import { runInAction } from 'mobx';
import { SearchBox, FlexDiv } from '../utils/styleUtils';
import { MemoryHeaderStrategy } from '../utils/strategyUtils';
import type { CardRankInfo, Session } from '../entity/session';
import { MemorySession, GroupBy } from '../entity/memorySession';
import { Label, useHit } from './Common';
import { Select } from '@insight/lib/components';
import { getRankInfoLabel, GroupCardRankInfosByHost, notNull, transformCardIdInfo } from '@insight/lib/utils';
import { useTranslation } from 'react-i18next';
import _ from 'lodash';

const groupByOptions = (isCompare: boolean): Array<{ label: string; value: string }> => {
    const options = [
        { label: 'Overall', value: 'Overall' },
        { label: 'Stream', value: 'Stream' },
        { label: 'Component', value: 'Component' },
    ];
    if (isCompare) {
        options.splice(1, 1);
    }
    return options;
};

const MemoryHeader = observer(({ strategy, session, memorySession }:
{strategy: MemoryHeaderStrategy; session: Session; memorySession: MemorySession}) => {
    const isCompare: boolean = session.compareRank.isCompare;
    const { t } = useTranslation('memory');
    const hit = useHit();
    const groupByCondition = groupByOptions(isCompare).map(item => ({
        ...item,
        label: t(`searchCriteria.${item.label}`),
    }));

    const getRankOptions = (value: string, ranks?: Map<string, CardRankInfo[]>): Array<CardRankInfo & { value: number }> => {
        const tempRanks = _.cloneDeep(ranks);
        // 将db格式中rankId内的host名称剔除，只对RankId为数字做排序，不能转为数字的字符串则不排序
        return (tempRanks?.get(value) ?? []).sort((a, b) => a.index - b.index).map((item, idx) => ({ ...item, value: idx }));
    };

    const onHostChanged = (value: string): void => {
        const rankOptions = getRankOptions(value, memorySession.hostCondition.cardsMap);
        runInAction(() => {
            memorySession.hostCondition = { ...memorySession.hostCondition, value };
            memorySession.rankCondition = { options: rankOptions, value: 0 };
        });
    };

    const onRankValueChanged = (value: number): void => {
        runInAction(() => {
            memorySession.rankCondition = { ...memorySession.rankCondition, value };
        });
    };

    const onGroupByChanged = (value: string): void => {
        runInAction(() => {
            memorySession.groupId = value as GroupBy;
        });
    };

    useEffect(() => {
        const { hosts, cardsMap } = GroupCardRankInfosByHost(session.memoryCardInfos);
        // 判断是否为db场景（host存在compareRank中)，若是则取出host，否则hostCondition.value为空
        // rankId 实际是 cardId
        const cardIdInfo = transformCardIdInfo(session.compareRank.rankId);
        const host = notNull(memorySession.hostCondition.value) ? memorySession.hostCondition.value : cardIdInfo.host;
        const rankOptions = getRankOptions(host, cardsMap);
        runInAction(() => {
            memorySession.hostCondition = { ...memorySession.hostCondition, options: hosts, cardsMap };
            if (!hosts.includes(host)) {
                memorySession.hostCondition.value = '';
            } else {
                memorySession.hostCondition.value = host ?? '';
            }
            memorySession.rankCondition.options = rankOptions;
            const foundIdx = rankOptions.findIndex(({ rankInfo }) => rankInfo.rankId === session.compareRank.rankId);
            memorySession.rankCondition.value = foundIdx >= 0 ? foundIdx : undefined;
        });
    }, [session.memoryCardInfos]);

    useEffect(() => {
        runInAction(() => {
            memorySession.rankCondition.options = [...memorySession.rankCondition.options]; // 为了刷新页面
        });
    }, [session.isAllMemoryCompletedSwitch]);

    useEffect(() => {
        const hostSetted = memorySession.hostCondition.options.length === 0 || memorySession.hostCondition.value !== '';
        if (session.compareRank.rankId === memorySession.selectedRankId && hostSetted) {
            return;
        }
        // rankId 实际是 cardId
        const cardIdInfo = transformCardIdInfo(session.compareRank.rankId);
        if (cardIdInfo.host !== '') {
            const rankOptions = getRankOptions(cardIdInfo.host, memorySession.hostCondition.cardsMap);
            const hostCondition =
                { ...memorySession.hostCondition, value: memorySession.hostCondition.options.includes(cardIdInfo.host) ? cardIdInfo.host : '' };
            const foundIdx = rankOptions.findIndex(({ rankInfo }) => rankInfo.rankId === session.compareRank.rankId);
            runInAction(() => {
                memorySession.hostCondition = hostCondition;
                memorySession.rankCondition = {
                    options: rankOptions,
                    value: foundIdx >= 0 ? foundIdx : undefined,
                };
            });
        } else {
            runInAction(() => {
                memorySession.hostCondition.value = '';
            });
            const foundIdx = memorySession.rankCondition.options.findIndex(({ rankInfo }) => rankInfo.rankId === session.compareRank.rankId);
            if (foundIdx >= 0) {
                onRankValueChanged(foundIdx);
            }
        }
    }, [session.compareRank.rankId]);

    useEffect(() => {
        if (isCompare && memorySession.groupId === GroupBy.STREAM) {
            runInAction(() => {
                memorySession.groupId = GroupBy.DEFAULT;
            });
        }
    }, [isCompare]);
    const renderHostField = (): { key: string; element: JSX.Element } => {
        return {
            key: 'host',
            element:
                <FlexDiv>
                    <Label name={t('searchCriteria.Host')} />
                    <Select
                        id={'select-host'}
                        value={memorySession.hostCondition.value}
                        size="middle"
                        onChange={onHostChanged}
                        disabled={isCompare}
                        options={memorySession.hostCondition.options.map((host: string) => {
                            return { value: host, label: host };
                        })}
                    />
                </FlexDiv>,
        };
    };
    const renderRankField = (): { key: string; element: JSX.Element } => {
        return {
            key: 'rankId',
            element:
                <FlexDiv>
                    <Label name={t('searchCriteria.RankId')} />
                    <Select
                        id={'select-rankId'}
                        value={memorySession.rankCondition.value}
                        size="middle"
                        onChange={(value: number): void => {
                            onRankValueChanged(value);
                        }}
                        disabled={isCompare}
                        options={memorySession.rankCondition.options.map((item) => {
                            return {
                                value: item.value,
                                label: getRankInfoLabel(item.rankInfo),
                            };
                        })}
                    />
                </FlexDiv>,
        };
    };
    const renderGroupIdField = (): { key: string; element: JSX.Element } => {
        return {
            key: 'groupId',
            element:
                <FlexDiv>
                    <Label name={<span>{t('searchCriteria.GroupBy')}{hit}</span>} />
                    <Select
                        id={'select-groupId'}
                        value={memorySession.groupId}
                        style={{ width: 180 }}
                        onChange={onGroupByChanged}
                        options={groupByCondition}
                    />
                </FlexDiv>,
        };
    };
    const renderFields = (): JSX.Element[] => {
        const fields = [
            renderHostField(),
            renderRankField(),
            renderGroupIdField(),
        ];
        return fields
            .filter((field) => strategy.shouldDisplay(field.key))
            .map((field) => <React.Fragment key={field.key}>{field.element}</React.Fragment>);
    };

    return (
        <div className="mb-30" style={{ paddingLeft: '24px', paddingRight: '24px' }}>
            <SearchBox>{renderFields()}</SearchBox>
        </div>
    );
});

export default MemoryHeader;
