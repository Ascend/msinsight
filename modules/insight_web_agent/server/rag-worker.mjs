/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { parentPort, workerData } from "node:worker_threads";
import { createRagService } from "./services/rag/ragService.mjs";
import { notifyRagDiagnostic, ragStatusReason } from "./services/rag/ragDiagnostic.mjs";

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
            let diagnostic;
            const onDiagnostic = (value) => { diagnostic = value; };
            try {
                const result = await service.retrieve(message.query, { ...message.options, onDiagnostic });
                parentPort.postMessage({ type: "result", id: message.id, result, ...(diagnostic ? { diagnostic } : {}) });
            } catch (error) {
                if (!diagnostic && error?.code !== "rag_busy") notifyRagDiagnostic(onDiagnostic, "retrieval_failed");
                parentPort.postMessage({ type: "result", id: message.id, error: error?.code === "rag_busy" ? "rag_busy" : "rag_retrieval_failed", ...(diagnostic ? { diagnostic } : {}) });
            }
        });
    }
} catch (error) {
    parentPort.postMessage({ type: "failed", reason: ragStatusReason(error?.code, "rag_load_failed") });
    parentPort.close();
}
