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
import React, { useEffect } from 'react';
import { observer } from 'mobx-react';
import { Button, Tooltip } from '@insight/lib/components';
import { Alert } from 'antd';
import type { Scope } from '../domain/types';
import type { Services } from './services';
import { useCommunicationStore } from '../store/context';
import { useQuery, type Query } from '../state/useQuery';
import { useTranslation } from 'react-i18next';
import { Selector } from '../components/Selector';
import { Section, Result } from '../components/Section';
import { TimelineSection } from '../sections/TimelineSection';
import { FluctuationSection } from '../sections/FluctuationSection';
import { MatrixSection } from '../sections/MatrixSection';
import { RankStatisticsSection } from '../sections/RankStatisticsSection';
import { AdviceSection } from '../sections/AdviceSection';
import { GroupOverview } from '../sections/GroupOverview';
import { ScopeFilterRow } from './styles';

export function Analysis({ services, iterations, iterationId }: {
    services: Services; iterations: Query<string[]>; iterationId: string;
}): JSX.Element {
    const { t } = useTranslation('communication');
    return <>
        <Section title={t('modern.groupAnalysis')}>
            <Result query={iterations} empty={!iterationId}>{() => <GroupOverview api={services.api} iterationId={iterationId} />}</Result>
        </Section>
        {iterationId && <GroupAnalysis key={iterationId} services={services} />}
        <AdviceSection api={services.api} />
    </>;
}

const GroupAnalysis = observer(function GroupAnalysis({ services }: { services: Services }): JSX.Element {
    const { t } = useTranslation('communication');
    const store = useCommunicationStore();
    const { clusterPath, generation, iterationId, locationIntent, group, scope } = store;
    const groups = useQuery(JSON.stringify([clusterPath, iterationId]), true, signal => services.api.groups(clusterPath, iterationId, signal));
    useEffect(() => {
        if (groups.data) { store.resolveGroup(generation, iterationId, groups.data); }
    }, [store, generation, iterationId, locationIntent, groups.data]);
    return <><ScopeFilterRow><Selector label={t('searchCriteria.CommunicationGroup')} value={group?.key ?? ''}
        options={(groups.data ?? []).map(item => ({ label: item.label, value: item.key, ranks: item.stage }))}
        dropdownMatchSelectWidth optionRender={option => (
            <Tooltip placement="right" title={option.ranks} overlayInnerStyle={{ maxHeight: '40vh', overflowY: 'auto', overflowWrap: 'anywhere' }}>
                <div style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{option.label}</div>
            </Tooltip>
        )}
        loading={groups.loading} onChange={value => {
            const selected = groups.data?.find(item => item.key === value);
            if (selected) { store.selectGroup(selected); }
        }} />
    {groups.error && <Alert type="error" message={t('modern.failed')} action={<Button type="primary" onClick={groups.retry}>{t('framework:clusterView.retry')}</Button>} />}</ScopeFilterRow>
    {scope && <ScopedCharts key={scope.group.key} scope={scope} services={services} />}</>;
});

/** Each section subscribes to its own shared fields within the selected scope. */
const ScopedCharts = React.memo(function ScopedCharts({ scope, services }: { scope: Scope; services: Services }): JSX.Element {
    const { api } = services;
    return <div>
        <TimelineSection scope={scope} services={services} />
        <FluctuationSection scope={scope} api={api} />
        <MatrixSection scope={scope} api={api} />
        <RankStatisticsSection scope={scope} api={api} />
    </div>;
});
