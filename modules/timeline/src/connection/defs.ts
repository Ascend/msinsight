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
// Timeline Module
import type { ImportCardInfo } from '../components/ImportSelect';

export const CONTENT_LENGTH_PREFIX = 'Content-Length';
export const PORT = 9000;

export interface ImportResult {
    reset: boolean;
    isPending: boolean;
    isSimulation: boolean;
    isIE: boolean;
    isIpynb: boolean;
    isCluster: boolean;
    isMultiDevice: boolean; // 判断是否是单Host多Device项目
    result: ImportCardInfo[];
}

export interface Request {
    id: number;
    method: string;
    params: Record<string, unknown>;
};

export interface Response<T = Record<string, unknown>> {
    id: number;
    result?: T;
    error?: {
        code: number;
        message: string;
    };
};

export interface Notification<T = Record<string, unknown>> {
    method: string;
    params: T;
};

export type ResponseHandler = (res: Response) => void;

export type NotificationHandler = (notification: Record<string, unknown>) => void;

export const isResopnse = (msg: Response | Notification): msg is Response => {
    return (msg as Response).id !== undefined;
};
