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
const diagnosticReasons = new Set([
    "embedding_unavailable", "index_unavailable", "knowledge_pack_unavailable",
    "worker_unavailable", "retrieval_failed",
]);
const statusReasons = new Set([
    ...diagnosticReasons, "disabled", "initializing", "unavailable", "closed",
    "unsupported_rag_platform", "pack_unreadable", "checksum_mismatch", "active_pointer_invalid",
    "load_failed", "rag_load_failed", "rag_busy", "rag_retrieval_failed",
    "rag_worker_failed", "rag_worker_exited", "rag_worker_start_failed", "rag_worker_closed", "rag_worker_send_failed",
    "model_directory_missing", "model_file_digest_mismatch", "model_file_missing", "model_manifest_invalid",
    "native_runtime_contract_mismatch", "native_runtime_digest_mismatch", "native_runtime_load_failed",
    "native_runtime_manifest_invalid", "native_runtime_missing", "native_runtime_target_mismatch",
    "onnx_contract_mismatch", "onnx_initialization_failed", "tokenizer_invalid",
    "embedding_input_invalid", "embedding_output_invalid", "package_semantics_invalid", "package_sha256_mismatch",
    "configuration_error", "invalid_archive", "invalid_archive_entries", "archive_too_large",
    "lifecycle_busy", "rag_data_unwritable", "rollback_unavailable", "sidecar_invalid",
    "strict_json_invalid", "unsupported_package_schema", "version_conflict",
]);
export const ragStatusReason = (value, fallback = "unavailable") => statusReasons.has(value) ? value : fallback;
export const ragDiagnosticReason = (value, fallback = "retrieval_failed") => diagnosticReasons.has(value) ? value : fallback;
export const notifyRagDiagnostic = (callback, value) => {
    const reason = ragDiagnosticReason(value?.reason ?? value);
    try { callback?.(Object.freeze({ reason })); } catch { /* Diagnostics cannot change retrieval semantics. */ }
};
