/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { parentPort, workerData } from "node:worker_threads";
import { createRagService } from "./services/rag/ragService.mjs";

const logger = Object.fromEntries(["info", "warn", "debug"].map((level) => [level,
    (message) => parentPort.postMessage({ type: "log", level, message }),
]));

try {
    const service = await createRagService({ config: workerData.config, logger });
    parentPort.postMessage({ type: "status", status: service.getStatus() });
    if (!service.isEnabled()) {
        parentPort.close();
    } else {
        parentPort.on("message", async (message) => {
            if (message.type !== "retrieve") return;
            try {
                const result = await service.retrieve(message.query, message.options);
                parentPort.postMessage({ type: "result", id: message.id, result });
            } catch (error) {
                parentPort.postMessage({ type: "result", id: message.id, error: error?.code ?? "rag_retrieval_failed" });
            }
        });
    }
} catch (error) {
    parentPort.postMessage({ type: "failed", reason: error?.code ?? "rag_load_failed" });
    parentPort.close();
}
