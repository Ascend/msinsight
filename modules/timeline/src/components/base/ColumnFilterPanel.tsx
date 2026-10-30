/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
import styled from '@emotion/styled';
import { Space } from 'antd';
import { SearchOutlined } from '@ant-design/icons';
import { Button } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';

const Panel = styled.div`
    padding: 8px;
    .ant-select {
        font-family: inherit;
        font-size: 12px;
    }
    .filter-header, .filter-input {
        width: 168px;
        margin-bottom: 8px;
    }
    .filter-header {
        margin-bottom: 4px;
    }
    .filter-actions {
        display: flex;
        justify-content: center;
    }
`;

interface ColumnFilterPanelProps {
    header?: React.ReactNode;
    inputControl: React.ReactNode;
    onSearch: () => void;
    onReset: () => void;
}

export const ColumnFilterPanel = ({ header, inputControl, onSearch, onReset }: ColumnFilterPanelProps): JSX.Element => {
    const { t } = useTranslation('buttonText');
    return <Panel onKeyDown={(event): void => event.stopPropagation()}>
        {header !== undefined && <div className="filter-header">{header}</div>}
        <div className="filter-input">
            {inputControl}
        </div>
        <Space className="filter-actions">
            <Button type="primary" icon={<SearchOutlined />} size="small" style={{ width: 80 }} onClick={onSearch}>
                {t('Search')}
            </Button>
            <Button size="small" style={{ width: 80 }} onClick={onReset}>
                {t('Reset')}
            </Button>
        </Space>
    </Panel>;
};
