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
export const messagesFromExport = (exported) => {
    return (exported.messages ?? []).flatMap((message) => {
        const role = message.info?.role;
        if (role !== "user" && role !== "assistant") return [];

        const result = {
            id: message.info?.id ?? crypto.randomUUID(),
            role,
            text: "",
            images: [],
            ...(role === "assistant" ? { thinking: "" } : {}),
        };

        for (const part of message.parts ?? []) {
            if (part.type === "text") result.text += part.text ?? "";
            if (role === "user" && part.type === "file") {
                const image = imageFromFilePart(part);
                if (image) result.images.push(image);
            }
            if (role === "assistant" && part.type === "reasoning") {
                result.thinking += part.text ?? "";
            }
        }

        if (!result.images.length) delete result.images;
        return result.text || result.thinking || result.images?.length ? [result] : [];
    });
};

const imageFromFilePart = (part) => {
    const mimeType = String(part.mime ?? "");
    if (!mimeType.startsWith("image/")) return undefined;

    const data = imageDataFromUrl(part.url, mimeType);
    if (!data) return undefined;

    return {
        id: part.id ?? crypto.randomUUID(),
        name: part.filename ?? part.source?.path ?? "image",
        mimeType,
        data,
    };
};

const imageDataFromUrl = (url, mimeType) => {
    const value = String(url ?? "");
    const prefix = `data:${mimeType};base64,`;
    if (value.startsWith(prefix)) return value.slice(prefix.length);

    const genericDataUrl = /^data:([^;,]+);base64,(.*)$/s.exec(value);
    if (genericDataUrl?.[1]?.startsWith("image/")) return genericDataUrl[2];

    return "";
};
