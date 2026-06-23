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
import { test } from "node:test";
import { messagesFromExport } from "./exportMapper.mjs";

test("messagesFromExport restores image file parts", () => {
    const messages = messagesFromExport({
        messages: [{
            info: { id: "msg-1", role: "user" },
            parts: [
                { type: "text", text: "分析这个图片" },
                {
                    id: "part-1",
                    type: "file",
                    mime: "image/png",
                    filename: "clipboard",
                    url: "data:image/png;base64,iVBORw0KGgo=",
                },
            ],
        }],
    });

    assert.deepEqual(messages, [{
        id: "msg-1",
        role: "user",
        text: "分析这个图片",
        images: [{
            id: "part-1",
            name: "clipboard",
            mimeType: "image/png",
            data: "iVBORw0KGgo=",
        }],
    }]);
});

test("messagesFromExport preserves non-png image mime types", () => {
    const messages = messagesFromExport({
        messages: [{
            info: { id: "msg-1", role: "user" },
            parts: [
                {
                    id: "part-jpeg",
                    type: "file",
                    mime: "image/jpeg",
                    filename: "photo.jpg",
                    url: "data:image/jpeg;base64,/9j/4AAQSkZJRg==",
                },
                {
                    id: "part-webp",
                    type: "file",
                    mime: "image/webp",
                    filename: "image.webp",
                    url: "data:image/webp;base64,UklGRiIAAABXRUJQVlA=",
                },
                {
                    id: "part-svg",
                    type: "file",
                    mime: "image/svg+xml",
                    filename: "diagram.svg",
                    url: "data:image/svg+xml;base64,PHN2Zy8+",
                },
            ],
        }],
    });

    assert.deepEqual(messages[0].images, [
        {
            id: "part-jpeg",
            name: "photo.jpg",
            mimeType: "image/jpeg",
            data: "/9j/4AAQSkZJRg==",
        },
        {
            id: "part-webp",
            name: "image.webp",
            mimeType: "image/webp",
            data: "UklGRiIAAABXRUJQVlA=",
        },
        {
            id: "part-svg",
            name: "diagram.svg",
            mimeType: "image/svg+xml",
            data: "PHN2Zy8+",
        },
    ]);
});
