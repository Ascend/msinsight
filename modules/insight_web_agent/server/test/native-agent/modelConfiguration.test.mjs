/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { getNativeConfigurationError } from "../../native-agent/config/modelConfiguration.mjs";

const configured = {
    MSINSIGHT_NATIVE_PROVIDER: "openai-compatible",
    MSINSIGHT_NATIVE_API_KEY: "test-secret",
    MSINSIGHT_NATIVE_MODEL: "test-model",
    MSINSIGHT_NATIVE_BASE_URL: "https://example.invalid/v1",
};

test("native preflight identifies missing fields without returning credentials", () => {
    for (const [key, field] of [
        ["MSINSIGHT_NATIVE_PROVIDER", "provider"],
        ["MSINSIGHT_NATIVE_API_KEY", "apiKey"],
        ["MSINSIGHT_NATIVE_MODEL", "model"],
        ["MSINSIGHT_NATIVE_BASE_URL", "baseUrl"],
    ]) {
        const error = getNativeConfigurationError({ ...configured, [key]: " \t" });
        assert.equal(error.field, field);
        assert.match(error.message, /Agent Settings/);
        assert.equal(JSON.stringify(error).includes("test-secret"), false);
    }
    assert.equal(getNativeConfigurationError({ ...configured, MSINSIGHT_NATIVE_API_KEY: undefined }).field, "apiKey");
    assert.equal(getNativeConfigurationError({ ...configured, MSINSIGHT_NATIVE_PROVIDER: "unsupported" }).field, "provider");
});

test("native preflight accepts configured providers and their optional default endpoints", () => {
    assert.equal(getNativeConfigurationError(configured), undefined);
    for (const provider of ["openai", "anthropic"]) {
        assert.equal(getNativeConfigurationError({
            ...configured, MSINSIGHT_NATIVE_PROVIDER: provider, MSINSIGHT_NATIVE_BASE_URL: "",
        }), undefined);
    }
});
