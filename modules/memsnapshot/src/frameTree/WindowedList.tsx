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
import React, { useEffect, useRef, useState } from 'react';

interface WindowedListProps<T> {
    items: readonly T[];
    height: number;
    rowHeight: number;
    scrollTo?: number;
    scrollToken?: number;
    role?: string;
    tabIndex?: number;
    label: string;
    renderRow: (item: T, index: number) => React.ReactNode;
}
export function WindowedList<T>({ items, height, rowHeight, scrollTo, scrollToken, role, tabIndex, label, renderRow }: WindowedListProps<T>): JSX.Element {
    const ref = useRef<HTMLDivElement>(null);
    const [top, setTop] = useState(0);
    const count = Math.ceil(height / rowHeight) + 8;
    const start = Math.max(0, Math.min(Math.floor(top / rowHeight) - 4, items.length - count));
    useEffect(() => {
        if (scrollTo === undefined || scrollTo < 0) return;
        const next = scrollTo * rowHeight;
        if (ref.current) ref.current.scrollTop = next;
        setTop(next);
    }, [scrollTo, scrollToken, rowHeight]);
    return <div ref={ref} role={role} tabIndex={tabIndex} aria-label={label} onScroll={(event): void => setTop(event.currentTarget.scrollTop)}
        style={{ height, overflow: 'auto', position: 'relative' }}>
        <div style={{ height: items.length * rowHeight, minWidth: '100%', position: 'relative' }}>
            <div style={{ position: 'absolute', top: start * rowHeight, left: 0, right: 0 }}>
                {items.slice(start, start + count).map((item, index) => renderRow(item, start + index))}
            </div>
        </div>
    </div>;
}
