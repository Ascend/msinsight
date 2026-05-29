import React from "react";

export const safeStringify = (obj: any): string => {
    try {
        return JSON.stringify(obj, (key, value) => {
            // 1. 跳过已知危险属性
            if (['__emotion_real', 'window', 'parent', 'top', '_context'].includes(key)) {
                return '[Filtered]';
            }
            // 2. 跳过特殊对象
            if (value && typeof value === 'object') {
                if (value instanceof Window || value instanceof HTMLElement) {
                    return '[DOM Object]';
                }
                if (React.isValidElement(value)) {
                    return '[React Element]';
                }
                if (value?.tag !== undefined && value?.key !== undefined && value?.stateNode !== undefined) {
                    return '[React Fiber Node]';
                }
            }
            // 3. 通用循环检测（try-catch 保护）
            try {
                if (typeof value === 'object' && value !== null) {
                    // 安全检查对象可枚举性
                    if (Object.getOwnPropertyNames(value).includes('__proto__')) {
                        return '[Complex Object]';
                    }
                }
            } catch (err) {
                console.warn('Skipped unsafe object at key:', key);
                return '[Unserializable]';
            }
            return value;
        });
    } catch (error) {
        console.error('Safe stringify failed:', error);
        return '[Serialization Failed]';
    }
}
