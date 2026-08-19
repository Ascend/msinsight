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
import React from 'react';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';
import SwitchTheme from './SwitchTheme';
import SwitchLanguage from './SwitchLanguage';
import Version from './Version';
import InsightBotEntry from './InsightBotEntry';

const Container = styled.div`
  position: absolute;
  right: 0;
  margin: 6px 20px 0 0;
  display: flex;
  align-items: center;
  color: ${({ theme }): string => theme.textColorPrimary};
  > * {
    margin-left: 16px;
    cursor: pointer;
  }
  .size-20 > svg {
    width: 20px;
    height: 20px;
  }
`;

interface ToolBoxProps {
    insightBotActive: boolean;
    onInsightBotClick: () => void;
}

const Index = observer(({ insightBotActive, onInsightBotClick }: ToolBoxProps) => {
    return <Container>
        <SwitchTheme/>
        <SwitchLanguage/>
        <Version/>
        <InsightBotEntry active={insightBotActive} onClick={onInsightBotClick} />
    </Container>;
});

export default Index;
