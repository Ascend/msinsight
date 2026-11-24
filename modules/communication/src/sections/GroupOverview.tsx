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
import React, { useState } from 'react';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import type { CommunicationApi } from '../api';
import { TRANSPORTS } from '../domain/types';
import { Selector, options } from '../components/Selector';
import { Result } from '../components/Section';
import { BarChart } from '../features/BarChart';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';
import { Subtitle, TwoColumnGrid } from '../components/layout';

const ChartHeading = styled(Subtitle)({ lineHeight: '32px', margin: 0 });
const HeadingWithFilter = styled.div({
    display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 16, minHeight: 32,
});

export const GroupOverview = observer(function GroupOverview({ api, iterationId }: { api: CommunicationApi; iterationId: string }): JSX.Element {
    const { clusterPath, dark } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const [transport, setTransport] = useState('SDMA');
    const range = useQuery(JSON.stringify([clusterPath, iterationId]), true, signal => api.groupRange(clusterPath, iterationId, signal));
    const bandwidth = useQuery(JSON.stringify([clusterPath, iterationId, transport]), true, signal => api.groupBandwidth(clusterPath, iterationId, transport, signal));
    return <TwoColumnGrid>
        <div>
            <ChartHeading>{t('modern.rangeRatioAnalysis')}</ChartHeading>
            <Result query={range} empty={!range.data?.items.length}>
                {data => <BarChart data={data} dark={dark} metric={t('modern.rangeRatio')} />}
            </Result>
        </div>
        <div>
            <HeadingWithFilter>
                <ChartHeading>{t('sessionTitle.BandwidthAnalysis')}</ChartHeading>
                <Selector width={112} label={t('modern.bandwidthType')} value={transport} options={options([...TRANSPORTS])} onChange={setTransport} />
            </HeadingWithFilter>
            <Result query={bandwidth} empty={!bandwidth.data?.items.length}>
                {data => <BarChart data={data} dark={dark} metric={t('searchCriteria.Bandwidth')} unit="GB/s" />}
            </Result>
        </div>
    </TwoColumnGrid>;
});
