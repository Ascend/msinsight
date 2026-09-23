/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React from 'react';

interface OverallCategoryCellProps {
    name: string;
    suffix?: React.ReactNode;
    suffixFollowsName?: boolean;
}

export const OverallCategoryCell = ({ name, suffix, suffixFollowsName = false }: OverallCategoryCellProps): JSX.Element => {
    if (suffixFollowsName) {
        return <span data-testid="overall-category-content" style={{
            display: 'inline-flex',
            alignItems: 'center',
            minWidth: 0,
            width: 'calc(100% - 25px)',
            maxWidth: 'calc(100% - 25px)',
            verticalAlign: 'baseline',
        }}>
            <span title={name} style={{
                flex: '0 1 auto', minWidth: 0, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
            }}>{name}</span>
            {suffix && <span data-testid="overall-category-suffix" style={{
                display: 'inline-flex', alignItems: 'center', flex: '0 0 auto', marginLeft: 4,
            }}>
                {suffix}
            </span>}
        </span>;
    }
    return <span data-testid="overall-category-content" style={{
        display: 'inline-flex', alignItems: 'center', gap: 4, minWidth: 0, maxWidth: '100%', verticalAlign: 'middle',
    }}>
        <span title={name} style={{
            flex: '1 1 auto', minWidth: 0, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
        }}>{name}</span>
        {suffix && <span data-testid="overall-category-suffix" style={{ display: 'inline-flex', flex: '0 0 auto' }}>
            {suffix}
        </span>}
    </span>;
};
