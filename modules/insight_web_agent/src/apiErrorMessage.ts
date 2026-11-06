/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import i18n from '@insight/lib/i18n';
import messages from './i18n/apiErrors.json';
import type { ErrorResponseBody } from './apiErrors';

export const apiErrorMessage = (body?: ErrorResponseBody): string => {
    const detail = body?.code === 'validation_failed' && Array.isArray(body.details) ? body.details[0] : undefined;
    const key = String(detail?.code ?? body?.messageKey ?? body?.code ?? body?.error ?? '').toLowerCase();
    const entry = Object.hasOwn(messages, key) ? messages[key as keyof typeof messages] : messages.request_failed;
    const language = /^zh/i.test(i18n.language) ? 'zh' : 'en';
    const params = detail?.messageParams ?? body?.messageParams ?? body ?? {};
    return entry[language].replace(/\{(\w+)\}/g, (_match, name: string) => String(params[name as keyof typeof params] ?? ''));
};
