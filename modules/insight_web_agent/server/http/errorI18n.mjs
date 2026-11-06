/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import messages from "../../src/i18n/apiErrors.json" with { type: "json" };

// Resolve each request independently; never change process-wide language state.
export const resolveErrorLanguage = (header = "") => {
    const languages = String(header).split(",").map((entry) => {
        const [language, ...parameters] = entry.trim().split(";");
        const quality = parameters.find((value) => value.trim().startsWith("q="));
        return { language, quality: quality ? Number(quality.trim().slice(2)) : 1 };
    }).filter((entry) => Number.isFinite(entry.quality) && entry.quality > 0 && entry.quality <= 1)
        .sort((left, right) => right.quality - left.quality);
    for (const { language } of languages) {
        if (/^zh(?:[-_]|CN|$)/i.test(language)) {
            return "zh";
        }
        if (/^en(?:[-_]|US|$)/i.test(language)) {
            return "en";
        }
    }
    return "en";
};

export const localizeError = (body, language) => {
    const detail = body.code === "validation_failed" && Array.isArray(body.details) ? body.details[0] : undefined;
    const key = String(detail?.code ?? body.messageKey ?? body.code ?? "").toLowerCase();
    const params = detail?.messageParams ?? body.messageParams ?? body;
    const entry = Object.hasOwn(messages, key) ? messages[key] : messages.request_failed;
    const message = entry[language].replace(/\{(\w+)\}/g, (_match, name) => String(params[name] ?? ""));
    return {
        ...body,
        message,
        ...(Array.isArray(body.details) ? {
            details: body.details.map((item) => item && typeof item === "object" && item.code
                ? { ...item, message: localizeError(item, language).message } : item),
        } : {}),
    };
};
