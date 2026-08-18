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

import { Session } from '../session';

describe('Session Kernel MFU state', () => {
    it('requests availability only after the selected cluster finishes Step2', () => {
        const session = new Session();
        session.updateKernelMfuClusterContext('project', 'cluster-a', [
            { path: 'cluster-a', durationParsed: false },
        ]);

        expect(session.startKernelMfuAvailabilityRequest('cluster-a')).toBeUndefined();

        session.markKernelMfuDurationParsed('other-cluster');
        expect(session.startKernelMfuAvailabilityRequest('cluster-a')).toBeUndefined();

        session.markKernelMfuDurationParsed('cluster-a');
        const sequence = session.startKernelMfuAvailabilityRequest('cluster-a');
        expect(sequence).toBeDefined();
        expect(session.kernelMfuAvailabilityChecking).toBe(true);
        expect(session.startKernelMfuAvailabilityRequest('cluster-a')).toBeUndefined();
        expect(session.isCurrentKernelMfuAvailabilityRequest(sequence as number, 'cluster-a')).toBe(true);

        session.updateKernelMfuAvailability({ available: true });
        expect(session.kernelMfuAvailability).toBe(true);
        expect(session.kernelMfuAvailabilityChecking).toBe(false);
    });

    it('invalidates requests and clears availability when the cluster changes', () => {
        const session = new Session();
        session.updateKernelMfuClusterContext('project', 'cluster-a', [
            { path: 'cluster-a', durationParsed: true },
        ]);
        const sequence = session.startKernelMfuAvailabilityRequest('cluster-a') as number;
        const previousGeneration = session.kernelMfuProjectGeneration;

        session.updateKernelMfuClusterContext('project', 'cluster-b', [
            { path: 'cluster-b', durationParsed: true },
        ]);

        expect(session.kernelMfuProjectGeneration).toBe(previousGeneration + 1);
        expect(session.selectedClusterPath).toBe('cluster-b');
        expect(session.kernelMfuAvailability).toBeUndefined();
        expect(session.kernelMfuAvailabilityChecking).toBe(false);
        expect(session.isCurrentKernelMfuAvailabilityRequest(sequence, 'cluster-a')).toBe(false);
    });
});
