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

import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import React from 'react';
import { useTranslation } from 'react-i18next';
import { FlagIcon } from '@insight/lib/icon';
import { Session } from '../../entity/session';

const KeyInfoContainer = styled.div`
    position: absolute;
    right: 8px;
    bottom: 4px;
    pointer-events: none;
`;

const InnerContainer = styled.div`
    display: flex;
    padding: 2px 4px;
    ::before {
        content: "";
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: ${(props) => props.theme.bgColorLight};
        border-radius: 4px;
        opacity: 0.7;
        z-index: 0;
    }
`;

const KeyInfoItem = styled.div`
    display: flex;
    flex-direction: row;
    align-items: center;
    opacity: 0.9;
    z-index: 1;
`;

const Key = styled.kbd`
    margin: 0 4px;
    display: inline-block;
    width: 18px;
    height: 18px;
    font-weight: bold;
    line-height: 1.2;
    text-align: center;
    color: ${(props): string => props.theme.primaryColor};
    border: 1px solid ${(props): string => props.theme.primaryColor};
    border-radius: 3px;
`;

const Info = styled.div`
    margin-bottom: 1px;
    font-size: 10pt;
    display: flex;
    align-items: center;
`;

interface IKeyInfoTooltip {
    session: Session;
}

const KeyInfoTooltip: React.FC<IKeyInfoTooltip> = observer(({ session }) => {
    const { t } = useTranslation('timeline');
    const hasChildren = React.useMemo(() => {
        return session.showCreateFlagMarkKey;
    }, [session.showCreateFlagMarkKey]);
    return <KeyInfoContainer>
        <InnerContainer style={{ visibility: hasChildren ? 'visible' : 'hidden' }}>
            <KeyInfoItem style={{ display: session.showCreateFlagMarkKey ? 'block' : 'none' }}>
                <Info>
                    <span>{t('KeyInfo.Press')}</span>
                    <Key>K</Key>
                    <span>{t('KeyInfo.Create')}</span>
                    <FlagIcon style={{ margin: '0 2px' }} />
                </Info>
            </KeyInfoItem>
        </InnerContainer>
    </KeyInfoContainer>;
});

export default KeyInfoTooltip;
