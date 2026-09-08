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
