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
import assert from "node:assert/strict";
import test from "node:test";
import { createBladeProvider } from "../../native-agent/runtime/bladeRuntime.mjs";

test("missing supported provider configuration remains in diagnostic mode", () => {
    assert.equal(createBladeProvider({}), undefined);
    assert.equal(createBladeProvider({ MSINSIGHT_NATIVE_PROVIDER: "openai" }), undefined);
    assert.equal(createBladeProvider({
        MSINSIGHT_NATIVE_PROVIDER: "openai-compatible",
        MSINSIGHT_NATIVE_API_KEY: "key",
    }), undefined);
});

test("supported providers produce Blade configuration", () => {
    assert.deepEqual(createBladeProvider({
        MSINSIGHT_NATIVE_PROVIDER: "openai",
        MSINSIGHT_NATIVE_API_KEY: "key",
        MSINSIGHT_NATIVE_BASE_URL: "https://models.example/v1",
    }), { type: "openai", apiKey: "key", baseUrl: "https://models.example/v1" });
});

test("azure, google, and arbitrary providers fail with a stable diagnostic", () => {
    for (const provider of ["azure", "google", "custom-provider"]) {
        assert.throws(
            () => createBladeProvider({ MSINSIGHT_NATIVE_PROVIDER: provider }),
            (error) => error.message === `unsupported_provider:${provider}`,
        );
    }
});
