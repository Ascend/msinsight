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
export interface FetchParams {
    [key: string]: unknown;
    module?: string;
    event?: never;
}

interface RequestMessage {
    [key: string]: unknown;
    event: 'request';
    module?: string;
    id: number;
}

type Send = (message: RequestMessage, onError: (error: Error) => void) => void;
type Pending = { resolve: (reply: unknown) => void; reject: (error: unknown) => void };

// The host echoes a numeric ID, even when an iframe is replaced under the same name.
// Share one sequence within this document; each new document starts at a random 52-bit ID.
let nextId = Math.floor(Math.random() * 2 ** 52);

function abortError(): DOMException {
    return new DOMException('Request aborted', 'AbortError');
}

/** Matches host replies for one client. Cancelling only releases the caller's wait. */
export class RequestChannel {
    private readonly pending = new Map<number, Pending>();

    constructor(private readonly send: Send) {}

    fetch(params: FetchParams, signal?: AbortSignal): Promise<unknown> {
        return new Promise((resolve, reject) => {
            if (signal?.aborted) { reject(abortError()); return; }
            const id = nextId++;
            const abort = (): void => fail(abortError());
            const cleanup = (): void => {
                this.pending.delete(id);
                signal?.removeEventListener('abort', abort);
            };
            const fail = (reason: unknown): void => { cleanup(); reject(reason); };
            this.pending.set(id, { resolve: reply => { cleanup(); resolve(reply); }, reject: fail });
            signal?.addEventListener('abort', abort, { once: true });
            try {
                this.send({ ...params, event: 'request', id }, fail);
            } catch (error) { fail(error); }
        });
    }

    receive(reply: Record<string, unknown>): void {
        if (reply.args !== undefined || typeof reply.id !== 'number') { return; }
        const request = this.pending.get(reply.id);
        if (!request) { return; }
        const body = reply.body;
        if (typeof body === 'object' && body !== null && 'error' in body && body.error != null) {
            request.reject(body.error);
        } else {
            request.resolve(reply);
        }
    }

    cancelAll(): void {
        const reason = abortError();
        this.pending.forEach(request => request.reject(reason));
    }
}
