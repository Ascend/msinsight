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

type OpfsAvailabilityProbe = () => Promise<boolean>;
type FallbackApproval = () => Promise<void>;

export const createOpfsFallbackGuard = (
    probeAvailability: OpfsAvailabilityProbe,
    requestFallbackApproval: FallbackApproval,
): (() => Promise<void>) => {
    let availabilityConfirmed = false;
    let fallbackApproved = false;
    let pendingCheck: Promise<void> | undefined;

    return (): Promise<void> => {
        if (availabilityConfirmed || fallbackApproved) {
            return Promise.resolve();
        }
        if (!pendingCheck) {
            pendingCheck = probeAvailability()
                .then(async available => {
                    if (available) {
                        availabilityConfirmed = true;
                        return;
                    }
                    await requestFallbackApproval();
                    fallbackApproved = true;
                })
                .finally(() => {
                    pendingCheck = undefined;
                });
        }
        return pendingCheck;
    };
};
