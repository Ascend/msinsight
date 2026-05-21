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

const formatKBytes = (value: number): string => value.toFixed(3);

const statItemStyle: React.CSSProperties = {
    display: 'inline-flex',
    alignItems: 'center',
    gap: 4,
    padding: '2px 10px',
    borderRadius: 12,
    background: '#f5f7fa',
    border: '1px solid #e5e6eb',
    lineHeight: '24px',
};

const statValueStyle: React.CSSProperties = {
    fontWeight: 600,
    color: '#1d2129',
};

const PotentialLeakStats = observer(({ session }: { session: any }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { leakStats } = session;
    return (
        <div style={{ display: 'inline-flex', gap: 8, marginLeft: 24, alignItems: 'center', flexWrap: 'wrap' }}>
            <span style={statItemStyle}>
                <span>{t('unreleasedTotal')}</span>
                <span style={statValueStyle}>{formatKBytes(leakStats.totalSize)}</span>
            </span>
            <span style={statItemStyle}>
                <span>{t('unreleasedMax')}</span>
                <span style={statValueStyle}>{formatKBytes(leakStats.maxSize)}</span>
            </span>
            <span style={statItemStyle}>
                <span>{t('unreleasedMin')}</span>
                <span style={statValueStyle}>{formatKBytes(leakStats.minSize)}</span>
            </span>
            {leakStats.loading ? <span style={{ color: '#86909c' }}>{t('calculating')}</span> : <></>}
            {leakStats.error ? <span style={{ color: '#ff4d4f' }}>{t('leakStatsFailed')}</span> : <></>}
        </div>
    );
});

export default PotentialLeakStats;
