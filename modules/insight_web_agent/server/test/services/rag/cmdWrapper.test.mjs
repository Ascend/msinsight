/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";

test("Windows wrapper invokes only the sibling bundled CLI without injecting a data root", async () => {
    const content = await readFile(new URL("../../../../scripts/mindstudio-insight-rag.cmd", import.meta.url), "utf8");

    assert.match(content, /%~dp0rag-cli\.mjs/);
    assert.doesNotMatch(content, /--data-dir|rag-data|[A-Za-z]:\\/);
});
