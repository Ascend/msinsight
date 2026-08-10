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
const createStructuredDataProvider = ({ state }) => ({
    name: "structured",
    schemaVersion: "1.0",
    isApplicable: () => true,
    async produce() {
        // TODO: Refine hidden-context normalization after the schema is finalized.
        // For now, /api/context is the source of truth and its raw payload is forwarded unchanged.
        return { contentRefs: state.activeContext };
    },
});

export const createContextAssembler = ({ state }) => {
    const providers = [createStructuredDataProvider({ state })];
    return {
        async assemble(sessionContext) {
            const contextProviders = [];
            for (const provider of providers) {
                if (!provider.isApplicable(sessionContext)) continue;
                const { contentRefs } = await provider.produce(sessionContext);
                contextProviders.push({
                    name: provider.name,
                    schemaVersion: provider.schemaVersion,
                    contentRefs,
                });
            }
            return {
                schemaVersion: "1.0",
                ...(state.activeContext?.projectRoot ? { projectRoot: state.activeContext.projectRoot } : {}),
                session: {
                    id: sessionContext.sessionId,
                    agentId: sessionContext.agentId,
                    agentRuntime: sessionContext.runtime,
                    mode: sessionContext.mode,
                },
                contextProviders,
                hands: {
                    skills: [],
                    tools: ["msinsight_observe", "msinsight_listActions"],
                    actions: [],
                    permissions: { "msinsight_invokeAction": "approval_required" },
                },
            };
        },
    };
};
