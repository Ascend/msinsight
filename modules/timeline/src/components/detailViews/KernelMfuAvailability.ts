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

import { useEffect } from 'react';
import { queryKernelMfuAvailability } from '../../api/request';
import type { Session } from '../../entity/session';

interface KernelMfuAvailabilityProps {
    session: Session;
    enabled: boolean;
}

export const useKernelMfuAvailability = ({ session, enabled }: KernelMfuAvailabilityProps): void => {
    useEffect(() => {
        const clusterPath = session.selectedClusterPath;
        if (!enabled || !session.isCluster || clusterPath === '' || !session.kernelMfuDurationParsed) {
            return;
        }
        const sequence = session.startKernelMfuAvailabilityRequest(clusterPath);
        if (sequence === undefined) {
            return;
        }
        void queryKernelMfuAvailability({ clusterPath }).then((response) => {
            if (!session.isCurrentKernelMfuAvailabilityRequest(sequence, clusterPath)) {
                return;
            }
            session.updateKernelMfuAvailability(response);
        }).catch(() => {
            if (!session.isCurrentKernelMfuAvailabilityRequest(sequence, clusterPath)) {
                return;
            }
            session.markKernelMfuAvailabilityError();
        });
    }, [
        enabled,
        session,
        session.kernelMfuAvailability,
        session.kernelMfuAvailabilityChecking,
        session.kernelMfuDurationParsed,
        session.isCluster,
        session.kernelMfuProjectGeneration,
        session.selectedClusterPath,
    ]);
};
