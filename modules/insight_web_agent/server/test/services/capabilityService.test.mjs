/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { publicCapabilities, setAgentCapabilities } from "../../services/capabilityService.mjs";

test("standard ACP capability objects and extension metadata are normalized", () => {
    const state = {};
    setAgentCapabilities(state, {
        loadSession: true,
        sessionCapabilities: { list: {}, delete: {}, resume: {} },
    }, {
        "msinsight.dev/setConfigOption": true,
    });

    assert.deepEqual(publicCapabilities(state), {
        loadSession: true,
        session: {
            list: true,
            delete: true,
            resume: true,
            close: false,
            setConfigOption: true,
        },
    });
});
