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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

/** 功能：创建基于 stdin/stdout 的 ACP JSON-RPC 协议服务。 */
export const createAcpProtocolServer = ({ input = process.stdin, writeJson, handleRequest, beforeRequest, onClose }) => {
    let buffer = "";
    let closed = false;
    const pending = new Set();

    /** 功能：启动输入监听，并把完整 JSON-RPC 行交给请求处理器。 */
    const start = async () => {
        await beforeRequest?.();
        input.setEncoding("utf8");
        input.on("data", handleInputChunk);
        input.once("end", handleInputClose);
        input.once("close", handleInputClose);
    };

    /** 功能：累积 stdin 数据，按换行切分完整 JSON-RPC 消息并交给协议处理器。 */
    const handleInputChunk = (chunk) => {
        buffer += chunk;
        while (true) {
            const newline = buffer.indexOf("\n");
            if (newline === -1) break;
            const line = buffer.slice(0, newline).trim();
            buffer = buffer.slice(newline + 1);
            if (line) {
                const request = handleLine(line).finally(() => pending.delete(request));
                pending.add(request);
            }
        }
    };

    const handleInputClose = async () => {
        if (closed) return;
        closed = true;
        input.off("data", handleInputChunk);
        await Promise.allSettled(pending);
        await onClose?.();
    };

    /** 功能：解析一行 JSON-RPC 请求，等待准备工作后执行方法并输出结果或错误。 */
    const handleLine = async (line) => {
        let message;
        try {
            message = JSON.parse(line);
        } catch (_error) {
            return;
        }
        if (!message?.method || message.id === undefined) return;

        try {
            await beforeRequest?.();
            const result = await handleRequest(message.method, message.params ?? {});
            writeJson({ jsonrpc: "2.0", id: message.id, result });
        } catch (error) {
            writeJson({
                jsonrpc: "2.0",
                id: message.id,
                error: {
                    code: error.code ?? -32000,
                    message: error.message,
                },
            });
        }
    };

    return { start };
};
