/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { useEffect, useState } from 'react';
import { defaultTableControllerRegistry, type TableControllerRegistry } from './TableControllerRegistry';
import type { TableController } from './types';

export const useAgentTableController = (
    controller: TableController | null,
    registry: TableControllerRegistry = defaultTableControllerRegistry,
): string | undefined => {
    const [targetId, setTargetId] = useState<string>();

    useEffect(() => {
        if (!controller) {
            setTargetId(undefined);
            return;
        }
        const registration = registry.register(controller);
        setTargetId(registration.targetId);
        return registration.unregister;
    }, [controller, registry]);

    return targetId;
};
