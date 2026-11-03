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
import { observer } from 'mobx-react';
import type { MemSnapshotContext as Session } from '../state/MemSnapshotContext';

import { addAddressOffset, formatBytes } from '../common/utils/utils';

const RIGHT_MARGIN = 200;

const BOTTOM_MARGIN = 100;

const HoverItemContainer = styled.div`
    position: absolute;
    padding: 10px;
    pointer-events: none;
    border-radius: 5px;
    background: ${(props): string => props.theme.bgColorDark};
`;
export const StateHoverItem = observer(({ session, point }: { session: Session; point: { x: number; y: number } }): JSX.Element => {
    const { stateWorkerInfo: { hoverItem, renderOptions: { viewport } } } = session;
    const left = point.x + RIGHT_MARGIN > viewport.width ? point.x - RIGHT_MARGIN : point.x + 20;
    const top = point.y + BOTTOM_MARGIN > viewport.height ? point.y - BOTTOM_MARGIN : point.y;
    const block = hoverItem?.data.blocks[0];
    const size = hoverItem?.type === 'block' && block !== undefined ? block.size : hoverItem?.data.size;
    const address = hoverItem?.type === 'block' && block !== undefined
        ? addAddressOffset(hoverItem.data.address, block.offset)
        : hoverItem?.data.address;

    return <>
        {
            point.x < 0 || point.y < 0 || hoverItem === null
                ? <></>
                : <HoverItemContainer style={{ left, top }}>
                    <div>Type: {hoverItem.type}</div>
                    <div>Addr: {address}</div>
                    <div>Size: {formatBytes(size ?? 0)}</div>
                    <div>Stream: {hoverItem.data.stream}</div>
                    <div>Event: {hoverItem.data.allocOrMapEventId}</div>
                    {
                        block === undefined
                            ? <></>
                            : <div>Offset: {formatBytes(block.offset)}</div>
                    }
                </HoverItemContainer>
        }
    </>;
});
