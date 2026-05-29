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
import React, { useRef } from 'react';
import './Resizor.css';

// eslint-disable-next-line max-lines-per-function
export function Resizor(props: {
    onResize: (deltaX: number, width: number, nextWidth?: number) => void;
    style?: object;
}): JSX.Element {
    const divRef = useRef<HTMLDivElement>(null);
    let isDown = false;
    let offsetX: number;
    let initialWidth: number;
    let initialNextWidth: number;
    const handleMouseDown = (event: React.MouseEvent): void => {
        event.preventDefault();
        if (!isDown) {
            isDown = true;
            offsetX = event.clientX;
            const dom = divRef?.current?.parentNode as HTMLElement;
            if (dom !== undefined && dom !== null) {
                dom.style.pointerEvents = 'none';
                initialWidth = dom.offsetWidth;
                const nextBrother = dom.nextElementSibling as HTMLElement;
                if (nextBrother !== null) {
                    initialNextWidth = nextBrother.offsetWidth;
                }
            }
            window.addEventListener('mousemove', handleMouseMove);
            window.addEventListener('mouseup', handleMouseUp);
            // 如果当前在iframe中
            if (window.self !== window.top) {
                window.addEventListener('message', handleTopWindow);
            }
            // 如果页面中有iframe
            if (window.document.querySelectorAll('iframe').length !== 0) {
                window.document.querySelectorAll('iframe').forEach((item: HTMLIFrameElement) => {
                    // @ts-expect-error:可用格式
                    item.style['pointer-events'] = 'none';
                });
            }
        }
    };
    function handleMouseMove(event: MouseEvent): void {
        event.preventDefault();
        if (isDown && Boolean(divRef?.current)) {
            const deltaX = event.clientX - offsetX;
            const dom = divRef?.current?.parentNode as HTMLElement;
            const width = initialWidth + deltaX;
            const nextBrother = dom?.nextElementSibling;
            if (nextBrother !== null) {
                const nextWidth = initialNextWidth - deltaX;
                props.onResize(deltaX, width, nextWidth);
                return;
            }
            props.onResize(deltaX, width);
        }
    }
    function handleMouseUp(event?: MouseEvent): void {
        if (event !== undefined) {
            event.preventDefault();
        }
        isDown = false;
        offsetX = 0;
        window.removeEventListener('mousemove', handleMouseMove);
        window.removeEventListener('mouseup', handleMouseUp);
        const parentNode = divRef?.current?.parentNode as HTMLElement;
        if (parentNode !== null && parentNode !== undefined) {
            // @ts-expect-error:可用格式
            parentNode.style.pointerEvents = null;
        }
        // 如果当前在iframe中
        if (window.self !== window.top) {
            window.removeEventListener('message', handleTopWindow);
        }
        // 如果页面中有iframe
        if (window.document.querySelectorAll('iframe').length !== 0) {
            window.document.querySelectorAll('iframe').forEach((item: HTMLIFrameElement) => {
                // @ts-expect-error:可用格式
                item.style['pointer-events'] = 'auto';
            });
        }
    }
    function handleTopWindow(event?: MessageEvent): void {
        try {
            if (typeof event?.data !== 'string') {
                return;
            }
            const data = JSON.parse(event.data);
            if (data.from === 'framework' && data.event === 'mouseover') {
                handleMouseUp();
            }
        } catch (error) { /* empty */ }
    }

    return <div ref={divRef} className={'resizor'} onMouseDown={handleMouseDown} style={props.style ?? {}}></div>;
};
