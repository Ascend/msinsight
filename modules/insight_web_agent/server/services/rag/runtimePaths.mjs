/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { basename, dirname, join, resolve } from "node:path";

export const fixedRagPaths = (entryPath) => {
    const entryDirectory = dirname(resolve(entryPath));
    const bundleRoot = basename(entryDirectory) === "server"
        ? dirname(entryDirectory)
        : entryDirectory;
    const runtimeDir = join(bundleRoot, "rag-runtime");
    return Object.freeze({
        ragDataDir: join(bundleRoot, "rag-data"),
        runtimeDir,
        modelDir: join(runtimeDir, "models", "bge-small-zh-v1.5"),
    });
};
