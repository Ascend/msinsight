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

import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { Select } from '@insight/lib/components';
import { Label } from '../Common';
import type { Session } from '../../entity/session';
import { PerformanceChartConditions } from './Index';
import { observer } from 'mobx-react';
import { FormItem } from '../communication/Filter';

interface optionDataType {
    key?: string;
    label: React.ReactNode;
    value: string | number;
    data?: string[];
}

// Top可选项： 1、2、4、8.......n(All)
const getTopOptions = (count: number): optionDataType[] => {
    const logIndex = Math.ceil(Math.log2(count > 0 ? count : 1));
    const toplist = [];
    for (let i = 0; i < logIndex; i++) {
        toplist.push(Math.pow(2, i));
    }
    const topOptions: optionDataType[] = toplist.map(item => ({ value: item, label: item }));
    if (count > 0) {
        topOptions.push({ value: count, label: `${count} (All)` });
    }
    return topOptions;
};

const allOptionItem = {
    value: 'All',
    label: 'All',
};

const getRankGroupOptions = (session: Session, isPipeline: boolean): optionDataType[] => {
    let optionsData: string[] = [];

    if (isPipeline) {
        optionsData = session.ppCommunicationDomains;
    } else {
        optionsData = session.communicationDomains;
    }

    const options = optionsData.map((domain) => ({
        value: domain,
        label: `(${domain})`,
    }));

    if (!isPipeline) {
        options.unshift(allOptionItem);
    }

    return options;
};

interface FilterProps {
    session: Session;
    conditions: PerformanceChartConditions;
    isPipeline: boolean;
    onFilterChange: (val: PerformanceChartConditions) => void;
}
export const Filter = observer(({ session, conditions, isPipeline, onFilterChange }: FilterProps): JSX.Element => {
    const { t } = useTranslation('summary');
    const tOrderOptions = session.performanceChartsIndicators?.map((item) => {
        return {
            value: item.key,
            label: t(item.name),
        };
    });
    tOrderOptions?.unshift({
        value: 'rankId',
        label: t('Rank ID'),
    });
    const handleChange = <K extends keyof PerformanceChartConditions>(key: K, val: PerformanceChartConditions[K]): void => {
        onFilterChange({ ...conditions, [key]: val });
    };

    const stepOptions = session.stepList.map(item => ({ value: item, label: item }));
    stepOptions.unshift(allOptionItem);
    const baselineStepOptions = session.baselineStepList.map(item => ({ value: item, label: item }));
    baselineStepOptions.unshift(allOptionItem);

    const groupOptions = useMemo(() => {
        return getRankGroupOptions(session, isPipeline);
    }, [session.communicationDomains, isPipeline]);
    const topOptions = getTopOptions(session.arrangementRankCount);

    return (<div style={{ marginBottom: 24 }}>
        <Label name={t('Step')} />
        <Select
            id="select-step"
            value={conditions.step}
            style={{ width: 120 }}
            onChange={(val: string): void => handleChange('step', val)}
            options={stepOptions}
        />
        {
            session.isCompare &&
            <FormItem name={t('Baseline Step')}
                content={ <Select
                    id="select-baseline-step"
                    value={conditions.baselineStep}
                    style={{ width: 120 }}
                    onChange={(val: string): void => handleChange('baselineStep', val)}
                    options={baselineStepOptions}
                />}
            />
        }
        <Label name={t('RankGroup')}/>
        <Select
            id="select-rank-group"
            showSearch
            filterOption={(input, option): boolean =>
                (option?.label as string ?? '').toLocaleLowerCase().includes(input.toLowerCase())
            }
            value={conditions.group}
            style={{ width: 200 }}
            dropdownMatchSelectWidth
            onChange={(val: string): void => handleChange('group', val)}
            options={groupOptions}
        />
        {
            isPipeline
                ? <></>
                : <>
                    <Label name={t('OrderBy')}/>
                    <Select
                        id="select-order-by"
                        value={conditions.orderBy}
                        style={{ width: 280 }}
                        onChange={(val: string): void => handleChange('orderBy', val)}
                        options={tOrderOptions}
                    />
                    <Label name={t('Top')}/>
                    <Select
                        id="select-top"
                        value={conditions.top}
                        style={{ width: 120 }}
                        disabled={session.arrangementRankCount <= 1}
                        onChange={(val: string): void => handleChange('top', val)}
                        options={topOptions}
                    />
                </>
        }
    </div>);
});

export default Filter;
