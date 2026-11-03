/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { ClientConnector } from '@insight/lib/connection';

// Request isolation belongs to this application; backend routing remains unchanged.
export class MemoryConnector extends ClientConnector {
    private nextRequestId: number;
    private readonly pendingRequests = new Map<number, { resolve: (value: unknown) => void; reject: (error: unknown) => void }>();

    constructor(initialRequestId = crypto.getRandomValues(new Uint32Array(1))[0] * 0x100000) {
        super({ module: 'leaks' });
        this.nextRequestId = initialRequestId;
    }

    override async fetch(params: Parameters<ClientConnector['fetch']>[0]): Promise<unknown> {
        return new Promise((resolve, reject) => {
            const id = this.nextRequestId++;
            const fail = (error: unknown): void => { this.pendingRequests.delete(id); reject(error); };
            this.pendingRequests.set(id, { resolve, reject });
            try {
                this.send<string>({ ...params, id, event: 'request' }, fail);
            } catch (error) { fail(error); }
        });
    }

    protected override awaitFetch(response: MessageEvent): void {
        const pending = this.pendingRequests.get(response.data.id);
        if (!pending) return;
        this.pendingRequests.delete(response.data.id);
        const error = response.data.body?.error;
        if (error !== null && error !== undefined) pending.reject(error);
        else pending.resolve(response.data);
    }
}
