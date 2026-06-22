/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { ClientConnector } from '../connection';
import { errorCenter, WsError } from './ErrorCenter';
import { createSmartDebounceRequestFunc } from './createSmartDebounceRequestFunc';

export interface RequestOptions {
    silent?: boolean; // 不显示报错信息
}

/**
 * 创建请求方法
 * @param connector 各模块的 connector 实例
 */
export function createRequest(connector: ClientConnector) {
    return async (
        command: string,
        params: any,
        module?: string,
        options?: RequestOptions,
    ): Promise<any> => {
        try {
            const data = await connector.fetch({
                args: { command, params },
                module: module !== undefined ? module : String(command).split('/')[0]?.toLowerCase(),
            });

            return (data as any)?.body;
        } catch (error: any) {
            const wsError = new WsError(error.code, error.message);
            if (!options?.silent && error.code < 9000) {
                errorCenter.handleError(wsError);
            }

            throw wsError;
        }
    };
}

/**
 * 创建防抖请求方法
 * 在 requestData 基础上包装智能防抖，默认保留最后一次请求（trailing）。
 *
 * @param connector 各模块的 connector 实例
 * @param debounceOptions 防抖配置
 */
export function createDebounceRequest(
    connector: ClientConnector,
    debounceOptions?: {
        delay?: number;
        leading?: boolean;
    },
) {
    const request = createRequest(connector);
    const debounced = createSmartDebounceRequestFunc(
        async (command: string, params: any, module?: string, options?: RequestOptions) => {
            return request(command, params, module, options);
        },
        {
            delay: debounceOptions?.delay ?? 300,
            leading: debounceOptions?.leading ?? false,
            keyFn: (command: string, params: any) => {
                // 用 method + params 生成唯一 key，确保不同接口互不干扰
                return `${command}:${JSON.stringify(params)}`;
            },
        },
    );

    return debounced;
}
