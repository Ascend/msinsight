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

const BUSINESS_MODULE_IFRAME_SELECTOR = 'iframe:not(#AcpSession)';

export function getModuleFrames(root: ParentNode = document): HTMLIFrameElement[] {
    return Array.from(root.querySelectorAll<HTMLIFrameElement>(BUSINESS_MODULE_IFRAME_SELECTOR));
}

export function getTargetWindow(root: ParentNode = document): Window[] {
    return getModuleFrames(root).flatMap(frame => frame.contentWindow ? [frame.contentWindow] : []);
}
