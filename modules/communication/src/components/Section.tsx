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
import styled from '@emotion/styled';
import { Button, CollapsiblePanel, Empty, Spin } from '@insight/lib/components';
import { Alert } from 'antd';
import type { Query } from '../state/useQuery';
import { useTranslation } from 'react-i18next';
import { SectionTitle } from './layout';
import { AdvicePanel } from './AdvicePanel';

const SectionFrame = styled.section({ marginBottom: 16 });
const AdviceFrame = styled(AdvicePanel)({ marginBottom: 16 });
const Status = styled.div({
    minHeight: 180, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
});
export const EmptyStatus = Status.withComponent(Empty);

interface SectionProps { title: string; tools?: React.ReactNode; children: React.ReactNode; advice?: boolean }

export const Section = React.forwardRef<HTMLElement, SectionProps>(function Section({ title, tools, children, advice = false }, ref) {
    const Frame = advice ? AdviceFrame : SectionFrame;
    return <Frame ref={ref}>
        <CollapsiblePanel title={<SectionTitle>{title}</SectionTitle>} suffix={tools} style={advice ? { marginBottom: 0 } : undefined}
            headerStyle={{ background: 'transparent', padding: 0, marginBottom: advice ? 12 : 8 }}
            contentStyle={{ padding: advice ? 0 : '0 0 16px' }}>{children}</CollapsiblePanel>
    </Frame>;
});
export function Result<T>({ query, empty, children }: { query: Query<T>; empty?: boolean; children: (data: T) => React.ReactNode }): JSX.Element {
    const { t } = useTranslation('communication');
    if (query.loading) { return <Status role="status"><Spin tip={t('modern.parsing')} /></Status>; }
    if (query.error) {
        const code = 'code' in query.error ? query.error.code : undefined;
        const message = code === 3108 ? 'modern.statisticsUnsupported' : code === 3109 ? 'modern.statisticsInvalid' : 'modern.failed';
        return <Alert type="error" showIcon message={t(message)} action={<Button type="primary" onClick={query.retry}>{t('framework:clusterView.retry')}</Button>} />;
    }
    if (query.data === undefined || empty) { return <EmptyStatus image={Empty.PRESENTED_IMAGE_SIMPLE} description={t('No Data', { ns: 'translation' })} />; }
    return <>{children(query.data)}</>;
}
