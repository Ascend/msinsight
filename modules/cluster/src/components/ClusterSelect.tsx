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
import connector from '../connection';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';
import { Select } from '@insight/lib/components';
import type { Session } from '../entity/session';
import { useTheme } from '@emotion/react';
import { VoidFunction } from '../utils/interface';

const Space = styled.div`
    display: flex;
    flex-direction: row;
    align-items: center;
`;

const LoadingIcon = (props: { color: string }): JSX.Element => {
    return <div className={'loading'} style={{
        borderRightColor: props.color,
        borderBottomColor: props.color,
        borderLeftColor: props.color,
        marginLeft: '14px',
    }}></div>;
};

interface ClusterSelectOptionType {
    label: string;
    value: string;
    parsing: boolean;
}

export const ClusterSelect = observer(({ width, session, onChange }: { width?: number; session: Session; onChange?: VoidFunction }): JSX.Element => {
    const theme = useTheme();
    const clusterOptions = useMemo(() => {
        return session.clusterList.map(({ name, path, parsed, durationParsed }) => ({
            label: name,
            value: path,
            parsing: !parsed || !durationParsed,
        }) as ClusterSelectOptionType);
    }, [session.clusterList]);

    return <Select<ClusterSelectOptionType>
        id={'cluster-select'}
        value={session.selectedClusterPath}
        style={{ width: width ?? 120 }}
        onChange={((val: string): void => {
            runInAction(() => {
                session.resetForClusterChange();
                session.selectedClusterPath = val;
            });
            connector.send({ event: 'clusterPage/selectCluster', body: { project: session.selectedProjectName, clusterPath: val } });
            onChange?.(val);
        })}
        options={clusterOptions}
        optionRender={(option: ClusterSelectOptionType): JSX.Element => {
            return <Space>
                <p style={{ flex: 1, margin: 0 }}>{option.label}</p>
                <span>{option.parsing && (<LoadingIcon color={theme.textColor} />)}</span>
            </Space>;
        }}
    >
    </Select>;
});
