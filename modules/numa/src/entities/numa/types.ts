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

export interface Metric {
    key: string;
    label: string;
    description: string;
    unit: string;
    value: number;
    hasValue?: boolean;
}

export interface NumaNode {
    id: number;
    name: string;
    metrics: Metric[];
}

export interface SocketNode {
    id: number;
    name: string;
    metrics: Metric[];
    numas: NumaNode[];
}

export type ConnectionType = 'memory' | 'numa' | 'socket';

export interface Connection {
    id: string;
    type: ConnectionType;
    source: string;
    target: string;
    label: string;
    description: string;
    metrics: Metric[];
}

export interface NumaOverview {
    range: { startTime: number; endTime: number };
    totalMetrics: Metric[];
    sockets: SocketNode[];
    connections: Connection[];
}

export interface DirectoryState {
    rankId: string;
    selectedFilePath: string;
}

export type Locale = 'zhCN' | 'enUS';

export type SelectedItem =
    | { kind: 'socket'; id: string; title: string; description: string; metrics: Metric[] }
    | { kind: 'numa'; id: string; title: string; description: string; metrics: Metric[] }
    | { kind: 'connection'; id: string; title: string; description: string; metrics: Metric[] };
