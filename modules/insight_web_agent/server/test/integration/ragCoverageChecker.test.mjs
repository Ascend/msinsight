/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { fileURLToPath } from "node:url";
import { test } from "node:test";
import {
    COVERAGE_GROUPS,
    validateCoverageMembership,
} from "../../../scripts/check-rag-coverage.mjs";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));

test("coverage checker freezes amended consumer group membership", () => {
    assert.deepEqual(validateCoverageMembership(packageRoot), COVERAGE_GROUPS);
    assert.deepEqual(COVERAGE_GROUPS["parser/contracts"], [
        "server/services/rag/wire/strictJsonParser.mjs",
        "server/services/rag/wire/canonicalJson.mjs",
        "server/services/rag/wire/packageContracts.mjs",
        "server/services/rag/runtimeContract.mjs",
    ]);
});

test("coverage checker rejects an unclassified critical contract file", () => {
    const incomplete = {
        ...COVERAGE_GROUPS,
        "parser/contracts": COVERAGE_GROUPS["parser/contracts"].filter((file) => !file.endsWith("runtimeContract.mjs")),
    };

    assert.throws(
        () => validateCoverageMembership(packageRoot, incomplete),
        /unclassified critical RAG files: server\/services\/rag\/runtimeContract\.mjs/,
    );
});
