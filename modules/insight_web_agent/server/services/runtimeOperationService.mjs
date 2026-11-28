/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

export const isRuntimeBusy = (state) => Boolean(state?.runtimeReloading)
    || [...(state?.sessionContexts?.values?.() ?? [])].some((context) => context?.pendingPrompt || context?.collectingHistory)
    || [...(state?.pendingPermissions?.values?.() ?? [])].some((request) => request?.state === "pending");

/** Claim synchronously, before any await, and keep the claim through rollback. */
export const withRuntimeReload = async (state, operation) => {
    if (isRuntimeBusy(state)) {
        throw Object.assign(new Error("Agent is busy"), { code: "agent_busy" });
    }
    state.runtimeReloading = true;
    try {
        return await operation();
    } finally {
        state.runtimeReloading = false;
    }
};
