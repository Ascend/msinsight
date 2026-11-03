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

type OpfsAvailabilityProbe = () => Promise<OpfsAvailabilityStatus>;
type FallbackApproval = () => Promise<void>;

interface OpfsFallbackGuard {
    ensureAvailability: () => Promise<void>;
    ensureFallbackApproval: () => Promise<void>;
}

export const createOpfsFallbackGuard = (
    probeAvailability: OpfsAvailabilityProbe,
    requestFallbackApproval: FallbackApproval,
): OpfsFallbackGuard => {
    let availabilityConfirmed = false;
    let fallbackApproved = false;
    let pendingCheck: Promise<void> | undefined;
    let pendingApproval: Promise<void> | undefined;

    const ensureFallbackApproval = (): Promise<void> => {
        if (fallbackApproved) {
            return Promise.resolve();
        }
        if (!pendingApproval) {
            pendingApproval = requestFallbackApproval()
                .then(() => {
                    fallbackApproved = true;
                })
                .finally(() => {
                    pendingApproval = undefined;
                });
        }
        return pendingApproval;
    };

    const ensureAvailability = (): Promise<void> => {
        if (availabilityConfirmed || fallbackApproved) {
            return Promise.resolve();
        }
        if (!pendingCheck) {
            pendingCheck = probeAvailability()
                .then(async status => {
                    if (status === 'available') {
                        availabilityConfirmed = true;
                        return;
                    }
                    if (status === 'unavailable') {
                        await ensureFallbackApproval();
                    }
                })
                .finally(() => {
                    pendingCheck = undefined;
                });
        }
        return pendingCheck;
    };

    return { ensureAvailability, ensureFallbackApproval };
};
