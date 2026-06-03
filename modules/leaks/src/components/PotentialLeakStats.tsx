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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';

const formatKBytes = (value: number): string => value.toFixed(3);

const PotentialLeakStats = observer(({ session }: { session: any }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { leakStats } = session;
    return (
        <StatsContainer>
            <StatItem>
                <span>{t('unreleasedTotal')}</span>
                <StatValue>{formatKBytes(leakStats.totalSize)}</StatValue>
            </StatItem>
            <StatItem>
                <span>{t('unreleasedMax')}</span>
                <StatValue>{formatKBytes(leakStats.maxSize)}</StatValue>
            </StatItem>
            <StatItem>
                <span>{t('unreleasedMin')}</span>
                <StatValue>{formatKBytes(leakStats.minSize)}</StatValue>
            </StatItem>
            {leakStats.loading ? <StatusText>{t('calculating')}</StatusText> : <></>}
            {leakStats.error ? <ErrorText>{t('leakStatsFailed')}</ErrorText> : <></>}
        </StatsContainer>
    );
});

const StatsContainer = styled.div`
    display: inline-flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 8px;
    margin-left: 24px;
`;

const StatItem = styled.span`
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 2px 10px;
    border: 1px solid ${(props): string => props.theme.mode === 'dark' ? '#3c3c3c' : '#e5e6eb'};
    border-radius: 12px;
    color: ${(props): string => props.theme.mode === 'dark' ? '#cccccc' : props.theme.textColorPrimary};
    background: ${(props): string => props.theme.mode === 'dark' ? '#2a2d2e' : '#f5f7fa'};
    line-height: 24px;
`;

const StatValue = styled.span`
    color: ${(props): string => props.theme.mode === 'dark' ? '#f0f0f0' : '#1d2129'};
    font-weight: 600;
`;

const StatusText = styled.span`
    color: ${(props): string => props.theme.mode === 'dark' ? '#9aa0a6' : '#86909c'};
`;

const ErrorText = styled.span`
    color: #ff4d4f;
`;

export default PotentialLeakStats;
