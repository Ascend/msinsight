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
import React from 'react';
import { observer } from 'mobx-react';
import { Alert } from 'antd';
import type { CommunicationApi } from '../api';
import type { Scope } from '../domain/types';
import { Result, Section } from '../components/Section';
import { BarChart } from '../features/BarChart';
import { SlowRanks } from '../features/slowRanks';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';

export const FluctuationSection = observer(function FluctuationSection({ scope, api }: {
    scope: Scope; api: CommunicationApi;
}): JSX.Element {
    const { dark, durationReady } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const key = JSON.stringify(scope);
    const fluctuation = useQuery(key, true, signal => api.fluctuation(scope, signal));
    const slow = useQuery(key, durationReady, signal => api.slowRanks(scope, signal));
    return <Section title={t('modern.fluctuation')}>
        <Result query={fluctuation} empty={!fluctuation.data?.items.length}>{data => (
            <BarChart data={data} dark={dark} metric={t('modern.elapsedTime')} unit="ms" />
        )}</Result>
        {durationReady
            ? <Result query={slow}>{data => <SlowRanks data={data} />}</Result>
            : <Alert type="info" message={t('modern.parsing')} />}
    </Section>;
});
