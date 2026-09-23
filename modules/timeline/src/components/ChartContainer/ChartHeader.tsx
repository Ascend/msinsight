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
import TimelineAxis from '../../components/charts/TimelineAxis';
import type { Session } from '../../entity/session';
import { ChartRow } from '../base/ChartRow';
import Recommendations from '../Recommendations';
import { ZoomTimestamp } from '../ZoomTimestamp';
import { ButtonGroup } from '../ButtonGroup';
import { TimeMakerAxis } from '../TimeMakerAxis';

const HeaderToolbar = styled.div<{ width?: number }>`
    display: flex;
    justify-content: space-between;
    box-sizing: border-box;
    padding-left: 10px;
    padding-right: 10px;
    width: ${(props): number | undefined => props.width}px;
    .title {
        font-size: 12px;
    }

    button{
        z-index: 10;
    }
`;

const HeaderLeft = observer(({ session, laneInfoWidth }: { session: Session; laneInfoWidth: number }) =>
    (<HeaderToolbar width={laneInfoWidth}>
        <ButtonGroup session={session} />
        <ZoomTimestamp session={session} />
    </HeaderToolbar>));

const RightHeader = observer(({ session, showRecommendation, timelineHeight }:
{ session: Session; showRecommendation: boolean; timelineHeight: number }): JSX.Element => {
    return (
        <div onMouseMove = { handleMouseMove } onMouseLeave = { handleMouseLeave }>
            <TimelineAxis session={session} margin={2} timelineHeight={timelineHeight} />
            { showRecommendation ? <Recommendations session={session}/> : <></> }
        </div>
    );
});

const handleMouseMove = (e: React.MouseEvent): void => {
    const existCanvas = document.getElementsByClassName('drawCanvas');
    for (let i = 0; i < existCanvas.length; i++) {
        existCanvas.item(i)?.dispatchEvent(new MouseEvent(e.nativeEvent.type, e.nativeEvent));
    }
};

const handleMouseLeave = (e: React.MouseEvent): void => {
    const existCanvas = document.getElementsByClassName('drawCanvas');
    for (let i = 0; i < existCanvas.length; i++) {
        existCanvas.item(i)?.dispatchEvent(new MouseEvent(e.nativeEvent.type, e.nativeEvent));
    }
};

const ChartHeader = observer(({ session, laneInfoWidth, showRecommendation, timelineHeight }:
{ session: Session; laneInfoWidth: number; showRecommendation: boolean; timelineHeight: number }) => {
    return (
        <div>
            <ChartRow className="timeStamp" leftWidth={laneInfoWidth} rightWidth={`calc(100% - ${laneInfoWidth}px)`}>
                <HeaderLeft session={session} laneInfoWidth={laneInfoWidth} />
                <RightHeader session={session} showRecommendation={showRecommendation} timelineHeight={timelineHeight} />
            </ChartRow>
            <TimeMakerAxis session={session} laneInfoWidth={laneInfoWidth}
                showRecommendation={showRecommendation} timelineHeight={timelineHeight} />
        </div>
    );
});

export default ChartHeader;
