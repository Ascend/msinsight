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
import * as React from 'react';
import { useTranslation } from 'react-i18next';
import { CollapseAllLanesIcon, ExpandAllLanesIcon } from '@insight/lib/icon';
import type { Session } from '../entity/session';
import { getAllUnitsToggleState, toggleAllUnits } from '../actions/actionExpandUnits';
import { TimeMakerButton } from './TimeMakerButton';
import { UnitsFilter } from './UnitsFilter';
import { CategorySearch } from './CategorySearch';
import { FilterLinkLine } from './FilterLinkLine';
import { CustomButton } from './base/StyledButton';

const Container = styled.div`
    display: flex;
    align-items: center;
    text-align: center;
    font-size: 12px;
    margin-right: 3px;
    svg {
        cursor: pointer;
    }
`;

const ToggleAllButton = styled(CustomButton)`
    &:hover,
    &:focus:not(:focus-visible) {
        color: ${(props): string => props.theme.textColorPrimary};
        background-color: ${(props): string => props.theme.buttonColor.unSuspendBGColor};
        border-color: transparent;
        box-shadow: none;
    }
`;

export const ButtonGroup = observer(({ session }: { session: Session }) => {
    const { t } = useTranslation('timeline');
    const unit = session.selectedUnits[0];
    const isRenderLink = !(session.isSimulation && session.areFlagEventsHidden);
    const toggleState = getAllUnitsToggleState(session);
    const isCollapse = toggleState === 'collapse';
    const tooltip = t(isCollapse ? 'Collapse all lanes' : 'Expand all lanes');
    return (<Container>
        <TimeMakerButton session={session} />
        <UnitsFilter session={session} />
        <CategorySearch session={session} />
        {isRenderLink && <FilterLinkLine session={session}/>}
        <ToggleAllButton
            data-testid={'tool-toggle-all-units'}
            aria-label={tooltip}
            tooltip={tooltip}
            icon={(isCollapse ? CollapseAllLanesIcon : ExpandAllLanesIcon) as any}
            onClick={(): void => toggleAllUnits(session)}
        />
        {session.buttons.map((_Button, index) => <_Button session={session} key={`${session.id}-${index}`} />)}
        {unit?.buttons?.map((_Button, index) => <_Button session={session} key={`${unit.name}-${index}`} />)}
    </Container>);
});
