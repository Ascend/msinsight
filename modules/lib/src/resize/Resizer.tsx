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

import React, { useRef } from 'react';

export type ResizerType = {
    style?: React.CSSProperties
    callback?: (moveWidthLength: number, moveHeightLength: number) => void
}

export const Resizer = ({ style, callback }: ResizerType): JSX.Element => {
    const resizeRef = useRef<HTMLDivElement>(null);
    let isDrag = false;
    let offsetX: number;
    let offsetY: number;

    const handleMouseDown = (event: any): void => {
        event.preventDefault();
        isDrag = true;
        offsetX = event.screenX;
        offsetY = event.screenY;
        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
    };

    const handleMouseMove = (event: MouseEvent): void => {
        event.preventDefault();
        if (isDrag) {
            const moveWidthLength = event.screenX - offsetX;
            const moveHeightLength = event.screenY - offsetY;
            offsetX = event.screenX;
            offsetY = event.screenY;
            if (callback) {
                callback(moveWidthLength, moveHeightLength);
            }
        }
    };

    const handleMouseUp = (event: MouseEvent): void => {
        if (event !== undefined) {
            event.preventDefault();
        }
        isDrag = false;
        document.removeEventListener('mousemove', handleMouseMove);
        document.removeEventListener('mouseup', handleMouseUp);
    };

    return <div ref={resizeRef} onMouseDown={handleMouseDown}
        style={{
            position: 'absolute',
            width: 3,
            height: '100%',
            cursor: 'w-resize',
            ...(style?.left !== undefined ? {} : { right: -1 }),
            ...(style?.bottom !== undefined ? {} : { top: 0 }),
            ...style,
        }}
    />;
};
