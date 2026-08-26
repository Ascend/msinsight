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
import { createCliCapability } from "./cliCapability.mjs";

const capabilityFactories = {
    cli: (definition, context) => createCliCapability({
        name: definition.name,
        description: definition.description,
        executable: definition.executable,
        cwd: context.cwd,
    }),
};

export const registerConfiguredCapabilities = ({ capabilityCenter, definitions = [], cwd }) => {
    for (const definition of definitions) {
        const factory = capabilityFactories[definition.type];
        if (!factory) throw new Error(`Unsupported configured capability type '${definition.type}'.`);
        capabilityCenter.register(factory(definition, { cwd }));
    }
};
