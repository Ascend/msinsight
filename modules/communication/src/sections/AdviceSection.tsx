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
import type { CommunicationApi } from '../api';
import { Result, Section } from '../components/Section';
import { ClusterAdvice } from '../features/advice/ClusterAdvice';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';

/** Keep cluster diagnostics mounted independently of iteration/group changes. */
export const AdviceSection = observer(function AdviceSection({ api }: { api: CommunicationApi }): JSX.Element | null {
    const { clusterPath, parsed } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const cluster = useQuery(clusterPath, parsed && !!clusterPath, signal => api.clusterAdvice(clusterPath, signal));
    const showCluster = cluster.loading || !!cluster.error || !!cluster.data?.length;
    if (!showCluster) { return null; }
    return <Section title={t('Advice')} advice>
        <Result query={cluster}>{data => <ClusterAdvice data={data} />}</Result>
    </Section>;
});
