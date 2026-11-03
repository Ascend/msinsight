/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */

interface RendererOwner { module: string }
interface DisposableRenderer { dispose: () => void }
interface Entry<T> { resource: T; leases: number; revision: number; disposed: boolean }

// Session identity and feature jointly identify a renderer; switching one owner cannot reset another.
export const createRendererRegistry = <O extends RendererOwner, T extends DisposableRenderer>(create: (owner: O) => T): {
    get: (owner: O) => T;
    peek: (owner: O) => T | undefined;
    retain: (owner: O) => () => void;
    disposeAll: () => void;
} => {
    const owners = new WeakMap<O, Map<string, Entry<T>>>();
    const live = new Set<Entry<T>>();
    const lookup = (owner: O): Entry<T> | undefined => owners.get(owner)?.get(owner.module);
    const createEntry = (owner: O): Entry<T> => {
        const entry = { resource: create(owner), leases: 0, revision: 0, disposed: false };
        const entries = owners.get(owner) ?? new Map<string, Entry<T>>();
        entries.set(owner.module, entry);
        owners.set(owner, entries);
        live.add(entry);
        return entry;
    };
    const dispose = (entry: Entry<T>): void => {
        if (entry.disposed) return;
        entry.disposed = true;
        live.delete(entry);
        entry.resource.dispose();
    };
    return {
        // Callers capture this instance before async work; a new mount gets a fresh resource.
        get: owner => {
            const entry = lookup(owner);
            return (entry && !entry.disposed ? entry : createEntry(owner)).resource;
        },
        peek: owner => lookup(owner)?.resource,
        retain: owner => {
            const previous = lookup(owner);
            const entry = previous && !previous.disposed ? previous : createEntry(owner);
            entry.leases++;
            entry.revision++;
            let released = false;
            return () => {
                if (released) return;
                released = true;
                entry.leases--;
                const revision = ++entry.revision;
                // StrictMode re-acquires synchronously and must keep the transferred canvas alive.
                queueMicrotask(() => {
                    if (entry.leases === 0 && entry.revision === revision) dispose(entry);
                });
            };
        },
        disposeAll: () => { for (const entry of live) dispose(entry); },
    };
};

const sessionIds = new WeakMap<object, number>();
let nextSessionId = 0;
export const getRendererSessionKey = (session: object): number => {
    let id = sessionIds.get(session);
    if (id === undefined) { id = ++nextSessionId; sessionIds.set(session, id); }
    return id;
};
