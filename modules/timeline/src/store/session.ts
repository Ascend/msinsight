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
import { makeAutoObservable } from 'mobx';
import { Session } from '../entity/session';

export class SessionStore {
    private _activeSession: Session | undefined;

    constructor() {
        makeAutoObservable(this);
        this._activeSession = new Session({
            id: 'entry',
            name: 'entry',
            phase: 'configuring',
            units: [],
            availableUnits: [],
            startRecordTime: 0,
            endTimeAll: undefined,
            isNsMode: true,
            isOverflowMaxSafeNumber: false,
        });
    }

    /**
     * The session store is locked if a session is in recording, users can't switch between sessions when session store is locked.
     *
     * @returns Whether session store is locked
     */
    get activeSession(): Session | undefined {
        return this._activeSession;
    }

    set activeSession(value: Session | undefined) {
        if (this._activeSession !== value && this._activeSession !== undefined) {
            this._activeSession.pmuCounterCache.clear();
            this._activeSession.pmuCounterGeneration++;
        }
        this._activeSession = value;
    }

    // creates a new session in the store.
    async newSession(conf?: Partial<Session> | Session): Promise<Session | undefined> {
        const session = new Session({
            id: 'entry',
            name: conf?.name,
            phase: conf?.phase ?? 'configuring',
            units: conf?.units ?? [],
            availableUnits: conf?.availableUnits ?? [],
            icon: conf?.icon,
            startRecordTime: conf?.startRecordTime,
            endTimeAll: conf?.endTimeAll,
            isNsMode: conf?.isNsMode,
            isOverflowMaxSafeNumber: false,
        });
        return session;
    }
}
