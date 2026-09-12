/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const ACP_RUNTIME_STATUSES = ['ready', 'missing-node', 'unsupported-node', 'start-failed', 'starting'] as const;

export type AcpRuntimeStatus = typeof ACP_RUNTIME_STATUSES[number];

export type AcpUnavailableReason = Exclude<AcpRuntimeStatus, 'ready'> | 'unreachable';

export const ACP_STATUS_I18N_KEY: Record<AcpUnavailableReason, string> = {
    'missing-node': 'missingNode',
    'unsupported-node': 'unsupportedNode',
    'start-failed': 'startFailed',
    starting: 'starting',
    unreachable: 'unreachable',
};

export const isAcpRuntimeStatus = (value: string | null | undefined): value is AcpRuntimeStatus => (
    Boolean(value && (ACP_RUNTIME_STATUSES as readonly string[]).includes(value))
);

export const showsNodeDownload = (reason: AcpUnavailableReason): boolean => (
    reason === 'missing-node' || reason === 'unsupported-node'
);

export const resolveAcpRuntimeStatus = (
    search: string = '',
    nodeEnv: string | undefined = 'production',
    hasApiBase = false,
): AcpRuntimeStatus => {
    const params = new URLSearchParams(search);
    const status = params.get('acpStatus');
    if (isAcpRuntimeStatus(status)) {
        return status;
    }
    if (nodeEnv === 'development' || hasApiBase) {
        return 'ready';
    }
    const acpPort = params.get('acpPort');
    const token = params.get('capabilityToken') || params.get('acpCapabilityToken');
    if (acpPort && token) {
        return 'ready';
    }
    return 'start-failed';
};
